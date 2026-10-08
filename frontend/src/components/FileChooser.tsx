interface FileChooserProps {

    fileName: string; //nombre del archivo seleccionado

    onFileSelect:(
        event: React.ChangeEvent<HTMLInputElement>
    ) => void; //funcion que se ejecuta cuando se selecciona un archivo
}

function FileChooser({
    fileName,
    onFileSelect,
}: FileChooserProps) {
    return (
        <section className="panel">
            <div className="panel-header">
                <div>
                    <p className="panel-label">Entrada de Archivo</p>
                    <h2 className="panel-title">Carga de comandos</h2>
                </div>
            </div>

            <label className="file-loader">
                <span className="file-loader-button">Seleccionar Archivo</span>
                <span className="file-loader-filename">{fileName || "Ningún archivo seleccionado"}</span>

                <input
                    className="file-loader-input"
                    type="file"
                    accept=".txt, text/plain, .smia"
                    onChange={onFileSelect}
                />
            </label>
            <p className="panel-help">
                Formato permitido: archivo de texto con extensión
                .txt o .smia
            </p>
        </section>
    );

}
export default FileChooser;