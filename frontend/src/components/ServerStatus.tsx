
interface ServerStatusProps {
    // indica si el servidor se encuentra online
    isOnline: boolean;
}

//manda info al backnd
function ServerStatus({ isOnline }: ServerStatusProps) {
    return (
        <div
            className={
                isOnline
                    ? "server-status server-status--online"
                    : "server-status server-status--offline"
            }
        >
            <span className="server-status__indicator" />

            <span className="server-status__text">
                Backend Server Status:{" "}
                {isOnline ? "Online" : "Offline"}
            </span>
        </div>
    );
}

export default ServerStatus;
