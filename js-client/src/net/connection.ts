import net from "node:net";
import { ConnectionClosedError, RequestTimeoutError } from "../errors";

interface PendingRequest {
    command: string;
    timeoutMs: number;
    resolve: (raw: string) => void;
    reject: (error: Error) => void;
}

interface ActiveRequest {
    resolve: (raw: string) => void;
    reject: (error: Error) => void;
    responseBuffer: string;
    timeoutHandle: NodeJS.Timeout;
    settleHandle?: NodeJS.Timeout;
}

export class PoudbConnection {
    private socket: net.Socket | null = null;
    private connected = false;
    private queue: PendingRequest[] = [];
    private active: ActiveRequest | null = null;

    public constructor(
        private readonly defaultTimeoutMs: number,
        private readonly settleTimeMs: number,
    ) {}

    public async connect(host: string, port: number): Promise<void> {
        if (this.connected) {
            return;
        }

        await new Promise<void>((resolve, reject) => {
            const socket = new net.Socket();
            const onError = (error: Error) => {
                socket.removeAllListeners();
                reject(error);
            };

            socket.once("error", onError);
            socket.connect(port, host, () => {
                socket.off("error", onError);
                this.socket = socket;
                this.connected = true;
                this.bindSocket(socket);
                resolve();
            });
        });
    }

    public async disconnect(): Promise<void> {
        if (!this.socket) {
            this.connected = false;
            return;
        }

        const socket = this.socket;
        this.socket = null;
        this.connected = false;

        await new Promise<void>((resolve) => {
            socket.once("close", () => resolve());
            socket.end();
        });
    }

    public isConnected(): boolean {
        return this.connected;
    }

    public send(command: string, timeoutMs?: number): Promise<string> {
        if (!this.socket || !this.connected) {
            return Promise.reject(new ConnectionClosedError());
        }

        return new Promise<string>((resolve, reject) => {
            this.queue.push({
                command,
                timeoutMs: timeoutMs ?? this.defaultTimeoutMs,
                resolve,
                reject,
            });
            this.processQueue();
        });
    }

    private bindSocket(socket: net.Socket): void {
        socket.on("data", (chunk: Buffer) => {
            if (!this.active) {
                return;
            }

            this.active.responseBuffer += chunk.toString("utf8");
            if (this.active.settleHandle) {
                clearTimeout(this.active.settleHandle);
            }

            this.active.settleHandle = setTimeout(() => {
                if (!this.active) {
                    return;
                }

                const current = this.active;
                this.active = null;
                clearTimeout(current.timeoutHandle);
                if (current.settleHandle) {
                    clearTimeout(current.settleHandle);
                }

                current.resolve(current.responseBuffer);
                this.processQueue();
            }, this.settleTimeMs);
        });

        const failPending = (error: Error) => {
            this.connected = false;
            this.socket = null;

            if (this.active) {
                clearTimeout(this.active.timeoutHandle);
                if (this.active.settleHandle) {
                    clearTimeout(this.active.settleHandle);
                }
                const active = this.active;
                this.active = null;
                active.reject(error);
            }

            while (this.queue.length > 0) {
                const req = this.queue.shift();
                if (req) {
                    req.reject(error);
                }
            }
        };

        socket.on("error", (error) => failPending(error));
        socket.on("close", () => failPending(new ConnectionClosedError()));
    }

    private processQueue(): void {
        if (!this.socket || !this.connected || this.active || this.queue.length === 0) {
            return;
        }

        const req = this.queue.shift();
        if (!req) {
            return;
        }

        const timeoutHandle = setTimeout(() => {
            if (!this.active) {
                return;
            }

            const active = this.active;
            this.active = null;
            if (active.settleHandle) {
                clearTimeout(active.settleHandle);
            }
            active.reject(new RequestTimeoutError());
            this.processQueue();
        }, req.timeoutMs);

        this.active = {
            resolve: req.resolve,
            reject: req.reject,
            responseBuffer: "",
            timeoutHandle,
        };

        const payload = `${req.command.trimEnd()}\n`;
        this.socket.write(payload, (error) => {
            if (!error) {
                return;
            }

            if (!this.active) {
                req.reject(error);
                return;
            }

            const active = this.active;
            this.active = null;
            clearTimeout(active.timeoutHandle);
            if (active.settleHandle) {
                clearTimeout(active.settleHandle);
            }
            req.reject(error);
            this.processQueue();
        });
    }
}
