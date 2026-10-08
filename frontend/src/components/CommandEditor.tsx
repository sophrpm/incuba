interface CommandEditorProps {
    value: string; //valor por default

    onChange: (value: string) => void; //funcion que se ejecuta cuando se edita
}

function CommandEditor({
    value,
    onChange,
}: CommandEditorProps) {
    return (
        <section className="panel-editor">
            <div className="panel-header">
                <div>
                    <p className="panel-label">Editor de Comandos</p>
                    <h2 className="panel-title">Comandos</h2>
                </div>
                <span className="panel-counter">{
                value ? value.split("\n").length : 0} líneas
                </span>
            </div>
            <textarea
                className="command-editor"
                value={value}
                placeholder="Escribe tus comandos aquí..."
                spellCheck={false}
                onChange={(event) => {onChange(event.target.value)}}
            />
        </section>
    );
}
export default CommandEditor;