#ifndef REPORT_MANAGER_HPP
#define REPORT_MANAGER_HPP

#include <string>

#include "structures/DiskStructures.hpp"
#include "structures/Ext2Structures.hpp"

using namespace std;

struct AppState;


//maneja la generacion de reportes
class ReportManager {
public:

    //genera el reporte solicitado
    bool generateReport(const string& name, const string& path, const string& id, const string& pathFileLs, const AppState& appState, string& message) const;

private:

    //reporte del mbr y particiones
    bool reportMBR(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte de distribucion del disco
    bool reportDisk(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte de inodos usados
    bool reportInode(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte de bloques usados
    bool reportBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte del bitmap de inodos
    bool reportBitmapInode(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte del bitmap de bloques
    bool reportBitmapBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte del arbol completo
    bool reportTree(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte del superbloque
    bool reportSuperBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const;

    //reporte del contenido de un archivo
    bool reportFile(const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message) const;

    //reporte de archivos y carpetas
    bool reportLs(const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message) const;

    //genera archivo dot y lo convierte al formato pedido
    bool generateGraphviz(const string& dotContent, const string& outputPath, string& message) const;

    //guarda reportes de texto
    bool generateTextFile(const string& content, const string& outputPath, string& message) const;

    //obtiene extension del archivo de salida
    string getExtension(const string& path) const;

    //crea carpetas de la ruta de salida
    bool createOutputDirectories(const string& path) const;

    //convierte fecha a texto
    string formatDate(time_t date) const;

    //convierte char fijo a string
    string charArrayToString(const char* text, int size) const;

    //agrega ebrs al reporte mbr
    bool addLogicalPartitionsMBR(const string& diskPath, const Partition& extendedPartition, string& dotContent) const;

    //agrega particiones logicas al reporte disk
    bool addLogicalPartitionsDisk(const string& diskPath, const Partition& extendedPartition, string& dotContent) const;

    //agrega un inodo al reporte tree
    bool addInodeTree(const string& diskPath, const SuperBlock& superBlock, int inodeIndex, string& dotContent) const;

    //agrega bloque carpeta al tree
    bool addFolderBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent) const;

    //agrega bloque archivo al tree
    bool addFileBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent) const;

    //agrega bloque de apuntadores al tree
    bool addPointerBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, int level, string& dotContent) const;

    //convierte permisos a formato rwx
    string permissionToText(const Inode& inode) const;
};


#endif
