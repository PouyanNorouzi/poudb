export class PoudbClientError extends Error {
    public readonly code: string;

    public constructor(code: string, message: string) {
        super(message);
        this.code = code;
        this.name = "PoudbClientError";
    }
}

export class ConnectionClosedError extends PoudbClientError {
    public constructor(message = "Connection is closed") {
        super("CONNECTION_CLOSED", message);
        this.name = "ConnectionClosedError";
    }
}

export class RequestTimeoutError extends PoudbClientError {
    public constructor(message = "Request timed out") {
        super("REQUEST_TIMEOUT", message);
        this.name = "RequestTimeoutError";
    }
}

export class ProtocolParseError extends PoudbClientError {
    public constructor(message = "Failed to parse server response") {
        super("PROTOCOL_PARSE_ERROR", message);
        this.name = "ProtocolParseError";
    }
}

export class ServerMessageError extends PoudbClientError {
    public constructor(message: string) {
        super("SERVER_ERROR", message);
        this.name = "ServerMessageError";
    }
}

export class ReconnectExhaustedError extends PoudbClientError {
    public constructor(message = "Reconnect attempts exhausted") {
        super("RECONNECT_EXHAUSTED", message);
        this.name = "ReconnectExhaustedError";
    }
}
