interface ButtonsProps {

    onAnalyze: () => void; //Se ejecuta en analizar

    onClear: () => void; //Se ejecuta en limpiar

    disabled: boolean; //Dice si hay contenido en el editor de comandos
}

function Buttons({
    onAnalyze,
    onClear,
    disabled,
}: ButtonsProps) {
    return (
        <div className="buttons">
            <button
                className="button-clear"
                onClick={onClear}
                disabled={disabled}
            >
                Limpiar Editor
            </button>

            <button
                className="button-analyze"
                onClick={onAnalyze}
                disabled={disabled}
            >
                Analizar Comandos
            </button>
        </div>
    );
}
export default Buttons;