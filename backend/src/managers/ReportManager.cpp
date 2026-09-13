#include "managers/ReportManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include "managers/DiskManager.hpp"
#include "managers/FileManager.hpp"
#include "managers/FileSystemManager.hpp"
#include "managers/MountManager.hpp"
#include "state/AppState.hpp"
#include "utils/BinaryUtils.hpp"
#include "utils/PathUtils.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//escapa caracteres que pueden romper una tabla html de graphviz
static string escapeGraphvizText(const string& text){
    string result;

    for (char character : text){
        if (character == '&'){
            result += "&amp;";
        } else if (character == '<'){
            result += "&lt;";
        } else if (character == '>'){
            result += "&gt;";
        } else if (character == '"'){
            result += "&quot;";
        } else if (character == '\n'){
            result += "<BR ALIGN='LEFT'/>";
        } else if (character == '\r'){
            continue;
        } else {
            result += character;
        }
    }

    return result;
}


//convierte porcentaje a texto con dos decimales
static string percentageText(double value){
    stringstream stream;

    stream << fixed << setprecision(2) << value;

    return stream.str();
}


//obtiene solamente la fecha
static string dateOnly(time_t value){
    if (value == 0){
        return "-";
    }

    tm* dateInfo = localtime(&value);

    if (dateInfo == nullptr){
        return "-";
    }

    stringstream stream;

    stream << setfill('0');
    stream << setw(2) << dateInfo->tm_mday << "/";
    stream << setw(2) << dateInfo->tm_mon + 1 << "/";
    stream << dateInfo->tm_year + 1900;

    return stream.str();
}


//obtiene solamente la hora
static string timeOnly(time_t value){
    if (value == 0){
        return "-";
    }

    tm* dateInfo = localtime(&value);

    if (dateInfo == nullptr){
        return "-";
    }

    stringstream stream;

    stream << setfill('0');
    stream << setw(2) << dateInfo->tm_hour << ":";
    stream << setw(2) << dateInfo->tm_min << ":";
    stream << setw(2) << dateInfo->tm_sec;

    return stream.str();
}


//genera el reporte solicitado
bool ReportManager::generateReport(const string& name, const string& path, const string& id, const string& pathFileLs, const AppState& appState, string& message) const {
    string reportName = StringUtils::toLower(name);
    string extension = getExtension(path);
    bool textReport = reportName == "file" || reportName == "bm_inode" || reportName == "bm_block";
    if ((textReport && extension != "txt") || (!textReport && extension != "dot" && extension != "svg" && extension != "png" && extension != "jpg" && extension != "jpeg" && extension != "pdf")){
        message = "Error: use .txt para file/bitmaps o .dot, .svg, .png, .jpg, .jpeg, .pdf para reportes graficos.";
        return false;
    }
    for (const MountedPartition& mounted : appState.mountedPartitions){
        if (PathUtils::canonicalPath(path) == mounted.path || PathUtils::canonicalPath(path + ".dot") == mounted.path){
            message = "Error: la salida del reporte no puede reemplazar un disco.";
            return false;
        }
    }

    if (reportName == "mbr"){
        return reportMBR(path, id, appState, message);
    }

    if (reportName == "disk"){
        return reportDisk(path, id, appState, message);
    }

    if (reportName == "inode"){
        return reportInode(path, id, appState, message);
    }

    if (reportName == "block"){
        return reportBlock(path, id, appState, message);
    }

    if (reportName == "bm_inode"){
        return reportBitmapInode(path, id, appState, message);
    }

    if (reportName == "bm_block"){
        return reportBitmapBlock(path, id, appState, message);
    }

    if (reportName == "tree"){
        return reportTree(path, id, appState, message);
    }

    if (reportName == "sb"){
        return reportSuperBlock(path, id, appState, message);
    }

    if (reportName == "file"){
        if (pathFileLs.empty()){
            message = "Error: el reporte file necesita -path_file_ls.";
            return false;
        }

        return reportFile(path, id, pathFileLs, appState, message);
    }

    if (reportName == "ls"){
        if (pathFileLs.empty()){
            message = "Error: el reporte ls necesita -path_file_ls.";
            return false;
        }

        return reportLs(path, id, pathFileLs, appState, message);
    }

    message = "Error: tipo de reporte no reconocido.";
    return false;
}


//genera reporte del mbr y ebr
bool ReportManager::reportMBR(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    DiskManager diskManager;
    MBR mbr;

    if (!diskManager.readMBR(mountedPartition.path, mbr)){
        message = "Error: no se pudo leer el MBR.";
        return false;
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "rankdir=LR;\n";
    dotContent += "node [shape=plaintext];\n";

    //tabla principal del mbr
    dotContent += "mbr [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='2'><b>MBR</b></td></tr>\n";
    dotContent += "<tr><td>mbr_tamano</td><td>" + to_string(mbr.mbr_tamano) + "</td></tr>\n";
    dotContent += "<tr><td>mbr_fecha_creacion</td><td>" + formatDate(mbr.mbr_fecha_creacion) + "</td></tr>\n";
    dotContent += "<tr><td>mbr_dsk_signature</td><td>" + to_string(mbr.mbr_dsk_signature) + "</td></tr>\n";
    dotContent += "<tr><td>dsk_fit</td><td>" + string(1, mbr.dsk_fit) + "</td></tr>\n";
    dotContent += "</table>\n";
    dotContent += ">];\n";

    //crea tabla para cada particion
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        const Partition& partition = mbr.mbr_partitions[partitionPos];

        string nodeName = "partition" + to_string(partitionPos);
        string partitionName = escapeGraphvizText(charArrayToString(partition.part_name, 16));
        string partitionId = escapeGraphvizText(charArrayToString(partition.part_id, 4));

        dotContent += nodeName + " [label=<\n";
        dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
        dotContent += "<tr><td colspan='2'><b>PARTITION " + to_string(partitionPos + 1) + "</b></td></tr>\n";
        dotContent += "<tr><td>part_status</td><td>" + string(1, partition.part_status) + "</td></tr>\n";
        dotContent += "<tr><td>part_type</td><td>" + string(1, partition.part_type) + "</td></tr>\n";
        dotContent += "<tr><td>part_fit</td><td>" + string(1, partition.part_fit) + "</td></tr>\n";
        dotContent += "<tr><td>part_start</td><td>" + to_string(partition.part_start) + "</td></tr>\n";
        dotContent += "<tr><td>part_s</td><td>" + to_string(partition.part_s) + "</td></tr>\n";
        dotContent += "<tr><td>part_name</td><td>" + partitionName + "</td></tr>\n";
        dotContent += "<tr><td>part_correlative</td><td>" + to_string(partition.part_correlative) + "</td></tr>\n";
        dotContent += "<tr><td>part_id</td><td>" + partitionId + "</td></tr>\n";
        dotContent += "</table>\n";
        dotContent += ">];\n";

        dotContent += "mbr -> " + nodeName + ";\n";

        //si es extendida agrega los ebr
        if (partition.part_type == 'E'){
            if (!addLogicalPartitionsMBR(mountedPartition.path, partition, dotContent)){
                message = "Error: no se pudieron leer los EBR.";
                return false;
            }
        }
    }

    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte MBR generado correctamente.";
    return true;
}


//genera reporte grafico del espacio del disco
bool ReportManager::reportDisk(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    DiskManager diskManager;
    MBR mbr;

    if (!diskManager.readMBR(mountedPartition.path, mbr)){
        message = "Error: no se pudo leer el MBR.";
        return false;
    }

    if (mbr.mbr_tamano <= 0){
        message = "Error: el disco tiene un tamaño invalido.";
        return false;
    }

    struct DiskSegment {
        string type;
        string name;
        int start;
        int size;
        const Partition* partition;
    };

    vector<Partition> partitions;

    //guarda particiones usadas
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        if (mbr.mbr_partitions[partitionPos].part_s > 0){
            partitions.push_back(mbr.mbr_partitions[partitionPos]);
        }
    }

    //ordena fisicamente las particiones
    sort(partitions.begin(), partitions.end(), [](const Partition& firstPartition, const Partition& secondPartition){
        return firstPartition.part_start < secondPartition.part_start;
    });

    vector<DiskSegment> segments;

    //mbr siempre esta al inicio
    segments.push_back({"MBR", "MBR", 0, static_cast<int>(sizeof(MBR)), nullptr});

    int currentPosition = static_cast<int>(sizeof(MBR));

    //agrega particiones y huecos libres
    for (const Partition& partition : partitions){
        if (partition.part_start > currentPosition){
            segments.push_back({"FREE", "Libre", currentPosition, partition.part_start - currentPosition, nullptr});
        }

        string partitionName = charArrayToString(partition.part_name, 16);

        if (partition.part_type == 'P'){
            segments.push_back({"PRIMARY", partitionName, partition.part_start, partition.part_s, &partition});
        } else {
            segments.push_back({"EXTENDED", partitionName, partition.part_start, partition.part_s, &partition});
        }

        currentPosition = partition.part_start + partition.part_s;
    }

    //agrega espacio libre al final
    if (currentPosition < mbr.mbr_tamano){
        segments.push_back({"FREE", "Libre", currentPosition, mbr.mbr_tamano - currentPosition, nullptr});
    }

    vector<double> shownPercentages;
    double accumulatedPercentage = 0.0;

    //calcula porcentajes para que visualmente sumen 100
    for (size_t segmentPos = 0; segmentPos < segments.size(); segmentPos++){
        double percentage;

        if (segmentPos + 1 == segments.size()){
            percentage = 100.0 - accumulatedPercentage;
        } else {
            double realPercentage = (static_cast<double>(segments[segmentPos].size) * 100.0) / mbr.mbr_tamano;
            percentage = round(realPercentage * 100.0) / 100.0;
            accumulatedPercentage += percentage;
        }

        shownPercentages.push_back(percentage);
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "node [shape=plaintext];\n";
    dotContent += "disk [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";

    //titulo como las figuras del documento
    dotContent += "<tr><td colspan='" + to_string(segments.size()) + "'><b>DISCO - " + to_string(mbr.mbr_tamano) + " bytes</b></td></tr>\n";
    dotContent += "<tr>\n";

    //crea una celda horizontal por segmento
    for (size_t segmentPos = 0; segmentPos < segments.size(); segmentPos++){
        const DiskSegment& segment = segments[segmentPos];
        double percentage = shownPercentages[segmentPos];

        int cellWidth = static_cast<int>(percentage * 10);

        if (cellWidth < 70){
            cellWidth = 70;
        }

        if (segment.type == "MBR"){
            dotContent += "<td width='" + to_string(cellWidth) + "'>";
            dotContent += "<b>MBR</b><br/>";
            dotContent += percentageText(percentage) + "%";
            dotContent += "</td>\n";
        }

        else if (segment.type == "FREE"){
            dotContent += "<td width='" + to_string(cellWidth) + "'>";
            dotContent += "<b>LIBRE</b><br/>";
            dotContent += to_string(segment.size) + " bytes<br/>";
            dotContent += percentageText(percentage) + "%";
            dotContent += "</td>\n";
        }

        else if (segment.type == "PRIMARY"){
            dotContent += "<td width='" + to_string(cellWidth) + "'>";
            dotContent += "<b>PRIMARIA</b><br/>";
            dotContent += escapeGraphvizText(segment.name) + "<br/>";
            dotContent += to_string(segment.size) + " bytes<br/>";
            dotContent += percentageText(percentage) + "%";
            dotContent += "</td>\n";
        }

        else if (segment.type == "EXTENDED"){
            const Partition& extended = *segment.partition;

            dotContent += "<td width='" + to_string(cellWidth) + "'>";
            dotContent += "<table border='0' cellborder='1' cellspacing='0'>\n";
            dotContent += "<tr><td><b>EXTENDIDA " + escapeGraphvizText(segment.name) + "</b><br/>";
            dotContent += percentageText(percentage) + "% del disco</td></tr>\n";
            dotContent += "<tr><td>\n";
            dotContent += "<table border='0' cellborder='1' cellspacing='0'><tr>\n";

            int extendedStart = extended.part_start;
            int extendedEnd = extended.part_start + extended.part_s;
            int ebrPosition = extendedStart;
            int innerPosition = extendedStart;

            //recorre los ebr dentro de la extendida
            while (ebrPosition >= extendedStart && ebrPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
                EBR ebr;

                if (!BinaryUtils::readStruct(mountedPartition.path, ebrPosition, ebr)){
                    message = "Error: no se pudo leer un EBR para el reporte disk.";
                    return false;
                }

                //espacio libre antes del ebr si existe
                if (ebrPosition > innerPosition){
                    int freeSize = ebrPosition - innerPosition;
                    double innerPercent = (static_cast<double>(freeSize) * 100.0) / extended.part_s;

                    dotContent += "<td>Libre<br/>";
                    dotContent += percentageText(innerPercent) + "% ext.</td>\n";
                }

                double ebrPercent = (static_cast<double>(sizeof(EBR)) * 100.0) / extended.part_s;

                dotContent += "<td><b>EBR</b><br/>";
                dotContent += percentageText(ebrPercent) + "% ext.</td>\n";

                innerPosition = ebrPosition + static_cast<int>(sizeof(EBR));

                //si el ebr representa una logica
                if (ebr.part_s > 0){
                    if (ebr.part_start > innerPosition){
                        int freeSize = ebr.part_start - innerPosition;
                        double innerPercent = (static_cast<double>(freeSize) * 100.0) / extended.part_s;

                        dotContent += "<td>Libre<br/>";
                        dotContent += percentageText(innerPercent) + "% ext.</td>\n";
                    }

                    string logicalName = escapeGraphvizText(charArrayToString(ebr.part_name, 16));
                    double logicalPercent = (static_cast<double>(ebr.part_s) * 100.0) / extended.part_s;

                    dotContent += "<td><b>LOGICA</b><br/>";
                    dotContent += logicalName + "<br/>";
                    dotContent += percentageText(logicalPercent) + "% ext.</td>\n";

                    innerPosition = ebr.part_start + ebr.part_s;
                }

                if (ebr.part_next == -1){
                    break;
                }

                if (ebr.part_next <= ebrPosition || ebr.part_next >= extendedEnd){
                    break;
                }

                //el espacio antes del siguiente ebr es libre
                if (ebr.part_next > innerPosition){
                    int freeSize = ebr.part_next - innerPosition;
                    double innerPercent = (static_cast<double>(freeSize) * 100.0) / extended.part_s;

                    dotContent += "<td>Libre<br/>";
                    dotContent += percentageText(innerPercent) + "% ext.</td>\n";

                    innerPosition = ebr.part_next;
                }

                ebrPosition = ebr.part_next;
            }

            //espacio libre final dentro de la extendida
            if (innerPosition < extendedEnd){
                int freeSize = extendedEnd - innerPosition;
                double innerPercent = (static_cast<double>(freeSize) * 100.0) / extended.part_s;

                dotContent += "<td>Libre<br/>";
                dotContent += percentageText(innerPercent) + "% ext.</td>\n";
            }

            dotContent += "</tr></table>\n";
            dotContent += "</td></tr>\n";
            dotContent += "</table>\n";
            dotContent += "</td>\n";
        }
    }

    dotContent += "</tr>\n";

    //segunda fila muestra inicio y fin de cada area
    dotContent += "<tr>\n";

    for (const DiskSegment& segment : segments){
        dotContent += "<td>";
        dotContent += "Inicio: " + to_string(segment.start) + "<br/>";
        dotContent += "Fin: " + to_string(segment.start + segment.size);
        dotContent += "</td>\n";
    }

    dotContent += "</tr>\n";
    dotContent += "</table>\n";
    dotContent += ">];\n";
    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte DISK generado correctamente.";
    return true;
}


//genera reporte de inodos utilizados
bool ReportManager::reportInode(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "rankdir=LR;\n";
    dotContent += "node [shape=plaintext];\n";

    int previousInode = -1;

    //solo muestra inodos utilizados
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        char bitmapValue;

        if (!fileSystemManager.readInodeBitmap(mountedPartition.path, superBlock, inodeIndex, bitmapValue)){
            message = "Error: no se pudo leer el bitmap de inodos.";
            return false;
        }

        if (bitmapValue != '1'){
            continue;
        }

        Inode inode;

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, inodeIndex, inode)){
            message = "Error: no se pudo leer un inodo.";
            return false;
        }

        string nodeName = "inode" + to_string(inodeIndex);

        dotContent += nodeName + " [label=<\n";
        dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
        dotContent += "<tr><td colspan='2'><b>INODO " + to_string(inodeIndex) + "</b></td></tr>\n";
        dotContent += "<tr><td>i_uid</td><td>" + to_string(inode.i_uid) + "</td></tr>\n";
        dotContent += "<tr><td>i_gid</td><td>" + to_string(inode.i_gid) + "</td></tr>\n";
        dotContent += "<tr><td>i_s</td><td>" + to_string(inode.i_s) + "</td></tr>\n";
        dotContent += "<tr><td>i_atime</td><td>" + formatDate(inode.i_atime) + "</td></tr>\n";
        dotContent += "<tr><td>i_ctime</td><td>" + formatDate(inode.i_ctime) + "</td></tr>\n";
        dotContent += "<tr><td>i_mtime</td><td>" + formatDate(inode.i_mtime) + "</td></tr>\n";

        for (int blockPos = 0; blockPos < 15; blockPos++){
            dotContent += "<tr><td>i_block[" + to_string(blockPos) + "]</td><td>" + to_string(inode.i_block[blockPos]) + "</td></tr>\n";
        }

        dotContent += "<tr><td>i_type</td><td>" + string(1, inode.i_type) + "</td></tr>\n";
        dotContent += "<tr><td>i_perm</td><td>" + charArrayToString(inode.i_perm, 3) + "</td></tr>\n";
        dotContent += "</table>\n";
        dotContent += ">];\n";

        if (previousInode != -1){
            dotContent += "inode" + to_string(previousInode) + " -> " + nodeName + ";\n";
        }

        previousInode = inodeIndex;
    }

    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte INODE generado correctamente.";
    return true;
}


//genera reporte de todos los bloques utilizados
bool ReportManager::reportBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "rankdir=LR;\n";
    dotContent += "node [shape=plaintext];\n";

    set<int> printedBlocks;

    //imprime un bloque de datos segun el tipo del inodo
    function<bool(int, char)> printDataBlock = [&](int blockIndex, char inodeType){
        if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
            return false;
        }

        if (printedBlocks.count(blockIndex) > 0){
            return true;
        }

        printedBlocks.insert(blockIndex);

        if (inodeType == '0'){
            FolderBlock folderBlock;

            if (!fileSystemManager.readFolderBlock(mountedPartition.path, superBlock, blockIndex, folderBlock)){
                return false;
            }

            dotContent += "block" + to_string(blockIndex) + " [label=<\n";
            dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
            dotContent += "<tr><td colspan='2'><b>BLOQUE CARPETA " + to_string(blockIndex) + "</b></td></tr>\n";

            for (int entryPos = 0; entryPos < 4; entryPos++){
                string entryName = escapeGraphvizText(charArrayToString(folderBlock.b_content[entryPos].b_name, 12));

                dotContent += "<tr><td>" + entryName + "</td><td>" + to_string(folderBlock.b_content[entryPos].b_inodo) + "</td></tr>\n";
            }

            dotContent += "</table>\n";
            dotContent += ">];\n";

            return true;
        }

        FileBlock fileBlock;

        if (!fileSystemManager.readFileBlock(mountedPartition.path, superBlock, blockIndex, fileBlock)){
            return false;
        }

        string content = escapeGraphvizText(charArrayToString(fileBlock.b_content, 64));

        dotContent += "block" + to_string(blockIndex) + " [label=<\n";
        dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
        dotContent += "<tr><td><b>BLOQUE ARCHIVO " + to_string(blockIndex) + "</b></td></tr>\n";
        dotContent += "<tr><td align='left'>" + content + "</td></tr>\n";
        dotContent += "</table>\n";
        dotContent += ">];\n";

        return true;
    };

    //recorre bloques de apuntadores hasta llegar a datos
    function<bool(int, int, char)> printPointerBlock = [&](int blockIndex, int level, char inodeType){
        if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
            return false;
        }

        PointerBlock pointerBlock;

        if (!fileSystemManager.readPointerBlock(mountedPartition.path, superBlock, blockIndex, pointerBlock)){
            return false;
        }

        if (printedBlocks.count(blockIndex) == 0){
            printedBlocks.insert(blockIndex);

            dotContent += "block" + to_string(blockIndex) + " [label=<\n";
            dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
            dotContent += "<tr><td colspan='2'><b>BLOQUE APUNTADORES " + to_string(blockIndex) + "</b></td></tr>\n";

            for (int pointerPos = 0; pointerPos < 16; pointerPos++){
                dotContent += "<tr><td>" + to_string(pointerPos) + "</td><td>" + to_string(pointerBlock.b_pointers[pointerPos]) + "</td></tr>\n";
            }

            dotContent += "</table>\n";
            dotContent += ">];\n";
        }

        //recorre cada apuntador usado
        for (int pointerPos = 0; pointerPos < 16; pointerPos++){
            int nextBlock = pointerBlock.b_pointers[pointerPos];

            if (nextBlock == -1){
                continue;
            }

            dotContent += "block" + to_string(blockIndex) + " -> block" + to_string(nextBlock) + ";\n";

            if (level == 1){
                if (!printDataBlock(nextBlock, inodeType)){
                    return false;
                }
            } else {
                if (!printPointerBlock(nextBlock, level - 1, inodeType)){
                    return false;
                }
            }
        }

        return true;
    };

    //recorre todos los inodos usados
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        char bitmapValue;

        if (!fileSystemManager.readInodeBitmap(mountedPartition.path, superBlock, inodeIndex, bitmapValue)){
            message = "Error: no se pudo leer el bitmap de inodos.";
            return false;
        }

        if (bitmapValue != '1'){
            continue;
        }

        Inode inode;

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, inodeIndex, inode)){
            message = "Error: no se pudo leer un inodo.";
            return false;
        }

        //12 apuntadores directos
        for (int blockPos = 0; blockPos < 12; blockPos++){
            if (inode.i_block[blockPos] == -1){
                continue;
            }

            if (!printDataBlock(inode.i_block[blockPos], inode.i_type)){
                message = "Error: no se pudo leer un bloque utilizado.";
                return false;
            }
        }

        //indirecto simple
        if (inode.i_block[12] != -1){
            if (!printPointerBlock(inode.i_block[12], 1, inode.i_type)){
                message = "Error: no se pudo leer el apuntador simple.";
                return false;
            }
        }

        //indirecto doble
        if (inode.i_block[13] != -1){
            if (!printPointerBlock(inode.i_block[13], 2, inode.i_type)){
                message = "Error: no se pudo leer el apuntador doble.";
                return false;
            }
        }

        //indirecto triple
        if (inode.i_block[14] != -1){
            if (!printPointerBlock(inode.i_block[14], 3, inode.i_type)){
                message = "Error: no se pudo leer el apuntador triple.";
                return false;
            }
        }
    }

    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte BLOCK generado correctamente.";
    return true;
}


//genera bitmap de inodos
bool ReportManager::reportBitmapInode(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    string content;

    //20 registros por linea
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        char value;

        if (!fileSystemManager.readInodeBitmap(mountedPartition.path, superBlock, inodeIndex, value)){
            message = "Error: no se pudo leer el bitmap de inodos.";
            return false;
        }

        content += value;

        if ((inodeIndex + 1) % 20 == 0){
            content += "\n";
        } else {
            content += " ";
        }
    }

    if (superBlock.s_inodes_count % 20 != 0){
        content += "\n";
    }

    if (!generateTextFile(content, outputPath, message)){
        return false;
    }

    message = "Reporte BM_INODE generado correctamente.";
    return true;
}


//genera bitmap de bloques
bool ReportManager::reportBitmapBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    string content;

    //20 registros por linea
    for (int blockIndex = 0; blockIndex < superBlock.s_blocks_count; blockIndex++){
        char value;

        if (!fileSystemManager.readBlockBitmap(mountedPartition.path, superBlock, blockIndex, value)){
            message = "Error: no se pudo leer el bitmap de bloques.";
            return false;
        }

        content += value;

        if ((blockIndex + 1) % 20 == 0){
            content += "\n";
        } else {
            content += " ";
        }
    }

    if (superBlock.s_blocks_count % 20 != 0){
        content += "\n";
    }

    if (!generateTextFile(content, outputPath, message)){
        return false;
    }

    message = "Reporte BM_BLOCK generado correctamente.";
    return true;
}


//genera arbol completo ext2
bool ReportManager::reportTree(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "rankdir=LR;\n";
    dotContent += "node [shape=plaintext];\n";

    set<int> printedInodes;
    set<int> printedBlocks;

    function<bool(int)> addInode;

    //agrega bloque de datos al arbol
    function<bool(int, char)> addDataBlock = [&](int blockIndex, char inodeType){
        if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
            return false;
        }

        if (printedBlocks.count(blockIndex) == 0){
            printedBlocks.insert(blockIndex);

            if (inodeType == '0'){
                FolderBlock folderBlock;

                if (!fileSystemManager.readFolderBlock(mountedPartition.path, superBlock, blockIndex, folderBlock)){
                    return false;
                }

                dotContent += "block" + to_string(blockIndex) + " [label=<\n";
                dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
                dotContent += "<tr><td colspan='2'><b>BLOQUE CARPETA " + to_string(blockIndex) + "</b></td></tr>\n";

                for (int entryPos = 0; entryPos < 4; entryPos++){
                    string entryName = escapeGraphvizText(charArrayToString(folderBlock.b_content[entryPos].b_name, 12));

                    dotContent += "<tr><td>" + entryName + "</td><td>" + to_string(folderBlock.b_content[entryPos].b_inodo) + "</td></tr>\n";
                }

                dotContent += "</table>\n";
                dotContent += ">];\n";
            } else {
                FileBlock fileBlock;

                if (!fileSystemManager.readFileBlock(mountedPartition.path, superBlock, blockIndex, fileBlock)){
                    return false;
                }

                string content = escapeGraphvizText(charArrayToString(fileBlock.b_content, 64));

                dotContent += "block" + to_string(blockIndex) + " [label=<\n";
                dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
                dotContent += "<tr><td><b>BLOQUE ARCHIVO " + to_string(blockIndex) + "</b></td></tr>\n";
                dotContent += "<tr><td align='left'>" + content + "</td></tr>\n";
                dotContent += "</table>\n";
                dotContent += ">];\n";
            }
        }

        //si es carpeta sigue hacia los hijos
        if (inodeType == '0'){
            FolderBlock folderBlock;

            if (!fileSystemManager.readFolderBlock(mountedPartition.path, superBlock, blockIndex, folderBlock)){
                return false;
            }

            for (int entryPos = 0; entryPos < 4; entryPos++){
                string entryName = charArrayToString(folderBlock.b_content[entryPos].b_name, 12);
                int childInode = folderBlock.b_content[entryPos].b_inodo;

                if (childInode == -1 || entryName == "." || entryName == ".."){
                    continue;
                }

                dotContent += "block" + to_string(blockIndex) + " -> inode" + to_string(childInode) + " [label=<" + escapeGraphvizText(entryName) + ">];\n";

                if (!addInode(childInode)){
                    return false;
                }
            }
        }

        return true;
    };

    //recorre los niveles de apuntadores
    function<bool(int, int, char)> addPointerBlock = [&](int blockIndex, int level, char inodeType){
        if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
            return false;
        }

        PointerBlock pointerBlock;

        if (!fileSystemManager.readPointerBlock(mountedPartition.path, superBlock, blockIndex, pointerBlock)){
            return false;
        }

        if (printedBlocks.count(blockIndex) == 0){
            printedBlocks.insert(blockIndex);

            dotContent += "block" + to_string(blockIndex) + " [label=<\n";
            dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
            dotContent += "<tr><td colspan='2'><b>APUNTADORES " + to_string(blockIndex) + "</b></td></tr>\n";

            for (int pointerPos = 0; pointerPos < 16; pointerPos++){
                dotContent += "<tr><td>" + to_string(pointerPos) + "</td><td>" + to_string(pointerBlock.b_pointers[pointerPos]) + "</td></tr>\n";
            }

            dotContent += "</table>\n";
            dotContent += ">];\n";
        }

        for (int pointerPos = 0; pointerPos < 16; pointerPos++){
            int nextBlock = pointerBlock.b_pointers[pointerPos];

            if (nextBlock == -1){
                continue;
            }

            dotContent += "block" + to_string(blockIndex) + " -> block" + to_string(nextBlock) + ";\n";

            if (level == 1){
                if (!addDataBlock(nextBlock, inodeType)){
                    return false;
                }
            } else {
                if (!addPointerBlock(nextBlock, level - 1, inodeType)){
                    return false;
                }
            }
        }

        return true;
    };

    //agrega inodo y sus bloques
    addInode = [&](int inodeIndex){
        if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
            return false;
        }

        if (printedInodes.count(inodeIndex) > 0){
            return true;
        }

        Inode inode;

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, inodeIndex, inode)){
            return false;
        }

        printedInodes.insert(inodeIndex);

        dotContent += "inode" + to_string(inodeIndex) + " [label=<\n";
        dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
        dotContent += "<tr><td colspan='2'><b>INODO " + to_string(inodeIndex) + "</b></td></tr>\n";
        dotContent += "<tr><td>i_uid</td><td>" + to_string(inode.i_uid) + "</td></tr>\n";
        dotContent += "<tr><td>i_gid</td><td>" + to_string(inode.i_gid) + "</td></tr>\n";
        dotContent += "<tr><td>i_s</td><td>" + to_string(inode.i_s) + "</td></tr>\n";
        dotContent += "<tr><td>i_atime</td><td>" + formatDate(inode.i_atime) + "</td></tr>\n";
        dotContent += "<tr><td>i_ctime</td><td>" + formatDate(inode.i_ctime) + "</td></tr>\n";
        dotContent += "<tr><td>i_mtime</td><td>" + formatDate(inode.i_mtime) + "</td></tr>\n";
        dotContent += "<tr><td>i_type</td><td>" + string(1, inode.i_type) + "</td></tr>\n";
        dotContent += "<tr><td>i_perm</td><td>" + charArrayToString(inode.i_perm, 3) + "</td></tr>\n";

        for (int blockPos = 0; blockPos < 15; blockPos++){
            dotContent += "<tr><td>i_block[" + to_string(blockPos) + "]</td><td>" + to_string(inode.i_block[blockPos]) + "</td></tr>\n";
        }

        dotContent += "</table>\n";
        dotContent += ">];\n";

        //apuntadores directos
        for (int blockPos = 0; blockPos < 12; blockPos++){
            int blockIndex = inode.i_block[blockPos];

            if (blockIndex == -1){
                continue;
            }

            dotContent += "inode" + to_string(inodeIndex) + " -> block" + to_string(blockIndex) + ";\n";

            if (!addDataBlock(blockIndex, inode.i_type)){
                return false;
            }
        }

        //simple
        if (inode.i_block[12] != -1){
            dotContent += "inode" + to_string(inodeIndex) + " -> block" + to_string(inode.i_block[12]) + ";\n";

            if (!addPointerBlock(inode.i_block[12], 1, inode.i_type)){
                return false;
            }
        }

        //doble
        if (inode.i_block[13] != -1){
            dotContent += "inode" + to_string(inodeIndex) + " -> block" + to_string(inode.i_block[13]) + ";\n";

            if (!addPointerBlock(inode.i_block[13], 2, inode.i_type)){
                return false;
            }
        }

        //triple
        if (inode.i_block[14] != -1){
            dotContent += "inode" + to_string(inodeIndex) + " -> block" + to_string(inode.i_block[14]) + ";\n";

            if (!addPointerBlock(inode.i_block[14], 3, inode.i_type)){
                return false;
            }
        }

        return true;
    };

    //empieza siempre desde la raiz
    if (!addInode(0)){
        message = "Error: no se pudo recorrer el arbol EXT2.";
        return false;
    }

    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte TREE generado correctamente.";
    return true;
}


//genera reporte del superbloque
bool ReportManager::reportSuperBlock(const string& outputPath, const string& id, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    stringstream magicStream;

    magicStream << "0x" << uppercase << hex << superBlock.s_magic;

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "node [shape=plaintext];\n";
    dotContent += "sb [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='2'><b>SUPER BLOQUE EXT2</b></td></tr>\n";
    dotContent += "<tr><td>s_filesystem_type</td><td>" + to_string(superBlock.s_filesystem_type) + "</td></tr>\n";
    dotContent += "<tr><td>s_inodes_count</td><td>" + to_string(superBlock.s_inodes_count) + "</td></tr>\n";
    dotContent += "<tr><td>s_blocks_count</td><td>" + to_string(superBlock.s_blocks_count) + "</td></tr>\n";
    dotContent += "<tr><td>s_free_blocks_count</td><td>" + to_string(superBlock.s_free_blocks_count) + "</td></tr>\n";
    dotContent += "<tr><td>s_free_inodes_count</td><td>" + to_string(superBlock.s_free_inodes_count) + "</td></tr>\n";
    dotContent += "<tr><td>s_mtime</td><td>" + formatDate(superBlock.s_mtime) + "</td></tr>\n";
    dotContent += "<tr><td>s_umtime</td><td>" + formatDate(superBlock.s_umtime) + "</td></tr>\n";
    dotContent += "<tr><td>s_mnt_count</td><td>" + to_string(superBlock.s_mnt_count) + "</td></tr>\n";
    dotContent += "<tr><td>s_magic</td><td>" + magicStream.str() + "</td></tr>\n";
    dotContent += "<tr><td>s_inode_s</td><td>" + to_string(superBlock.s_inode_s) + "</td></tr>\n";
    dotContent += "<tr><td>s_block_s</td><td>" + to_string(superBlock.s_block_s) + "</td></tr>\n";
    dotContent += "<tr><td>s_firts_ino</td><td>" + to_string(superBlock.s_firts_ino) + "</td></tr>\n";
    dotContent += "<tr><td>s_first_blo</td><td>" + to_string(superBlock.s_first_blo) + "</td></tr>\n";
    dotContent += "<tr><td>s_bm_inode_start</td><td>" + to_string(superBlock.s_bm_inode_start) + "</td></tr>\n";
    dotContent += "<tr><td>s_bm_block_start</td><td>" + to_string(superBlock.s_bm_block_start) + "</td></tr>\n";
    dotContent += "<tr><td>s_inode_start</td><td>" + to_string(superBlock.s_inode_start) + "</td></tr>\n";
    dotContent += "<tr><td>s_block_start</td><td>" + to_string(superBlock.s_block_start) + "</td></tr>\n";
    dotContent += "</table>\n";
    dotContent += ">];\n";
    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte SB generado correctamente.";
    return true;
}


//genera reporte del contenido de un archivo
bool ReportManager::reportFile(const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    //estado temporal root para poder leer reporte
    AppState reportState = appState;
    reportState.session.active = true;
    reportState.session.user = "root";
    reportState.session.uid = 1;
    reportState.session.gid = 1;
    reportState.session.partitionId = id;

    FileManager fileManager;
    string content;
    string fileMessage;

    if (!fileManager.readFile(internalPath, reportState, content, fileMessage)){
        message = "Error: no se pudo leer el archivo. " + fileMessage;
        return false;
    }

    string fileName = internalPath;
    size_t lastSlash = fileName.find_last_of('/');

    if (lastSlash != string::npos){
        fileName = fileName.substr(lastSlash + 1);
    }

    string reportContent;

    reportContent += "Nombre: " + fileName + "\n";
    reportContent += "Ruta: " + internalPath + "\n";
    reportContent += "Contenido:\n";
    reportContent += content;

    if (!generateTextFile(reportContent, outputPath, message)){
        return false;
    }

    message = "Reporte FILE generado correctamente.";
    return true;
}


//genera reporte ls
bool ReportManager::reportLs(const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message) const {
    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    AppState reportState = appState;
    reportState.session.active = true;
    reportState.session.user = "root";
    reportState.session.uid = 1;
    reportState.session.gid = 1;
    reportState.session.partitionId = id;

    FileManager fileManager;
    FileSystemManager fileSystemManager;

    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    //lee users.txt para obtener nombres de propietarios
    string usersContent;
    string fileMessage;

    if (!fileManager.readFile("/users.txt", reportState, usersContent, fileMessage)){
        message = "Error: no se pudo leer users.txt para el reporte ls.";
        return false;
    }

    vector<string> userLines = StringUtils::split(usersContent, '\n');

    //busca nombre de grupo usando gid
    auto getGroupName = [&](int gid){
        for (const string& line : userLines){
            if (line.empty()){
                continue;
            }

            vector<string> fields = StringUtils::split(line, ',');

            if (fields.size() != 3 || fields[1] != "G" || fields[0] == "0"){
                continue;
            }

            if (fields[0] == to_string(gid)){
                return fields[2];
            }
        }

        return string("GID ") + to_string(gid);
    };

    //busca usuario usando uid
    auto getUserName = [&](int uid){
        for (const string& line : userLines){
            if (line.empty()){
                continue;
            }

            vector<string> fields = StringUtils::split(line, ',');

            if (fields.size() != 5 || fields[1] != "U" || fields[0] == "0"){
                continue;
            }

            if (fields[0] == to_string(uid)){
                return fields[3];
            }
        }

        return string("UID ") + to_string(uid);
    };

    int targetInodeIndex = -1;
    Inode targetInode;

    if (!fileManager.findInodeByPath(internalPath, reportState, targetInodeIndex, targetInode, fileMessage)){
        message = fileMessage;
        return false;
    }

    string dotContent;

    dotContent += "digraph G {\n";
    dotContent += "node [shape=plaintext];\n";
    dotContent += "ls [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='8'><b>REPORTE LS - " + escapeGraphvizText(internalPath) + "</b></td></tr>\n";
    dotContent += "<tr>";
    dotContent += "<td><b>Permisos</b></td>";
    dotContent += "<td><b>Propietario</b></td>";
    dotContent += "<td><b>Grupo</b></td>";
    dotContent += "<td><b>Fecha Mod.</b></td>";
    dotContent += "<td><b>Hora Mod.</b></td>";
    dotContent += "<td><b>Tipo</b></td>";
    dotContent += "<td><b>Fecha Creacion</b></td>";
    dotContent += "<td><b>Nombre</b></td>";
    dotContent += "</tr>\n";

    //agrega una fila al reporte
    auto addRow = [&](const string& name, const Inode& inode){
        string owner = escapeGraphvizText(getUserName(inode.i_uid));
        string group = escapeGraphvizText(getGroupName(inode.i_gid));
        string type = inode.i_type == '0' ? "Carpeta" : "Archivo";

        dotContent += "<tr>";
        dotContent += "<td>" + permissionToText(inode) + "</td>";
        dotContent += "<td>" + owner + "</td>";
        dotContent += "<td>" + group + "</td>";
        dotContent += "<td>" + dateOnly(inode.i_mtime) + "</td>";
        dotContent += "<td>" + timeOnly(inode.i_mtime) + "</td>";
        dotContent += "<td>" + type + "</td>";
        dotContent += "<td>" + dateOnly(inode.i_ctime) + "</td>";
        dotContent += "<td>" + escapeGraphvizText(name) + "</td>";
        dotContent += "</tr>\n";
    };

    //si se solicito un archivo
    if (targetInode.i_type == '1'){
        string fileName = internalPath;

        size_t lastSlash = fileName.find_last_of('/');

        if (lastSlash != string::npos){
            fileName = fileName.substr(lastSlash + 1);
        }

        addRow(fileName, targetInode);
    }

    //si se solicito una carpeta
    else {
        vector<Content> entries;

        if (!fileManager.getDirectoryContent(internalPath, reportState, entries, fileMessage)){
            message = fileMessage;
            return false;
        }

        for (const Content& entry : entries){
            string entryName = charArrayToString(entry.b_name, 12);

            if (entryName == "." || entryName == ".."){
                continue;
            }

            if (entry.b_inodo < 0 || entry.b_inodo >= superBlock.s_inodes_count){
                continue;
            }

            Inode entryInode;

            if (!fileSystemManager.readInode(mountedPartition.path, superBlock, entry.b_inodo, entryInode)){
                continue;
            }

            addRow(entryName, entryInode);
        }
    }

    dotContent += "</table>\n";
    dotContent += ">];\n";
    dotContent += "}\n";

    if (!generateGraphviz(dotContent, outputPath, message)){
        return false;
    }

    message = "Reporte LS generado correctamente.";
    return true;
}


//ejecuta graphviz para crear imagen
bool ReportManager::generateGraphviz(const string& dotContent, const string& outputPath, string& message) const {
    if (!createOutputDirectories(outputPath)){
        message = "Error: no se pudieron crear las carpetas del reporte.";
        return false;
    }

    string extension = getExtension(outputPath);

    if (extension.empty()){
        message = "Error: la ruta del reporte necesita una extension.";
        return false;
    }

    //si pide dot guarda directamente el codigo
    if (extension == "dot"){
        return generateTextFile(dotContent, outputPath, message);
    }

    string dotPath = outputPath + ".dot";

    ofstream dotFile(dotPath);

    if (!dotFile.is_open()){
        message = "Error: no se pudo crear el archivo temporal Graphviz.";
        return false;
    }

    dotFile << dotContent;
    dotFile.close();

    string format = "-T" + extension;
    pid_t child = fork();
    if (child == 0){
        execlp("dot", "dot", format.c_str(), dotPath.c_str(), "-o", outputPath.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    int result = child < 0 || waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0;

    remove(dotPath.c_str());

    if (result != 0){
        message = "Error: Graphviz no pudo generar el reporte.";
        return false;
    }

    return true;
}


//genera archivo de texto
bool ReportManager::generateTextFile(const string& content, const string& outputPath, string& message) const {
    if (!createOutputDirectories(outputPath)){
        message = "Error: no se pudieron crear las carpetas del reporte.";
        return false;
    }

    ofstream file(outputPath);

    if (!file.is_open()){
        message = "Error: no se pudo crear el archivo del reporte.";
        return false;
    }

    file << content;
    file.close();

    return true;
}


//obtiene extension del archivo
string ReportManager::getExtension(const string& path) const {
    return StringUtils::toLower(PathUtils::getExtension(path));
}


//crea carpetas necesarias para el reporte
bool ReportManager::createOutputDirectories(const string& path) const {
    return PathUtils::createDirectories(path);
}


//convierte fecha y hora a texto
string ReportManager::formatDate(time_t date) const {
    if (date == 0){
        return "-";
    }

    tm* dateInfo = localtime(&date);

    if (dateInfo == nullptr){
        return "-";
    }

    stringstream dateStream;

    dateStream << setfill('0');
    dateStream << setw(2) << dateInfo->tm_mday << "/";
    dateStream << setw(2) << dateInfo->tm_mon + 1 << "/";
    dateStream << dateInfo->tm_year + 1900 << " ";
    dateStream << setw(2) << dateInfo->tm_hour << ":";
    dateStream << setw(2) << dateInfo->tm_min << ":";
    dateStream << setw(2) << dateInfo->tm_sec;

    return dateStream.str();
}


//convierte char fijo a string
string ReportManager::charArrayToString(const char* text, int size) const {
    return BinaryUtils::fixedCharToString(text, size);
}


//agrega ebr al reporte mbr
bool ReportManager::addLogicalPartitionsMBR(const string& diskPath, const Partition& extendedPartition, string& dotContent) const {
    int currentPosition = extendedPartition.part_start;
    int extendedEnd = extendedPartition.part_start + extendedPartition.part_s;

    while (currentPosition >= extendedPartition.part_start && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
        EBR ebr;

        if (!BinaryUtils::readStruct(diskPath, currentPosition, ebr)){
            return false;
        }

        string nodeName = "ebr" + to_string(currentPosition);

        dotContent += nodeName + " [label=<\n";
        dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
        dotContent += "<tr><td colspan='2'><b>EBR</b></td></tr>\n";
        dotContent += "<tr><td>Posicion EBR</td><td>" + to_string(currentPosition) + "</td></tr>\n";
        dotContent += "<tr><td>part_mount</td><td>" + string(1, ebr.part_mount) + "</td></tr>\n";
        dotContent += "<tr><td>part_fit</td><td>" + string(1, ebr.part_fit) + "</td></tr>\n";
        dotContent += "<tr><td>part_start</td><td>" + to_string(ebr.part_start) + "</td></tr>\n";
        dotContent += "<tr><td>part_s</td><td>" + to_string(ebr.part_s) + "</td></tr>\n";
        dotContent += "<tr><td>part_next</td><td>" + to_string(ebr.part_next) + "</td></tr>\n";
        dotContent += "<tr><td>part_name</td><td>" + escapeGraphvizText(charArrayToString(ebr.part_name, 16)) + "</td></tr>\n";
        dotContent += "</table>\n";
        dotContent += ">];\n";

        if (ebr.part_next != -1){
            dotContent += nodeName + " -> ebr" + to_string(ebr.part_next) + ";\n";
        }

        if (ebr.part_next == -1){
            break;
        }

        if (ebr.part_next <= currentPosition || ebr.part_next >= extendedEnd){
            break;
        }

        currentPosition = ebr.part_next;
    }

    return true;
}


//agrega logicas a una tabla de disk
bool ReportManager::addLogicalPartitionsDisk(const string& diskPath, const Partition& extendedPartition, string& dotContent) const {
    int currentPosition = extendedPartition.part_start;
    int extendedEnd = extendedPartition.part_start + extendedPartition.part_s;

    while (currentPosition >= extendedPartition.part_start && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
        EBR ebr;

        if (!BinaryUtils::readStruct(diskPath, currentPosition, ebr)){
            return false;
        }

        double ebrPercent = (static_cast<double>(sizeof(EBR)) * 100.0) / extendedPartition.part_s;

        dotContent += "<td>EBR<br/>" + percentageText(ebrPercent) + "% ext.</td>\n";

        if (ebr.part_s > 0){
            double logicalPercent = (static_cast<double>(ebr.part_s) * 100.0) / extendedPartition.part_s;
            string logicalName = escapeGraphvizText(charArrayToString(ebr.part_name, 16));

            dotContent += "<td>Logica<br/>" + logicalName + "<br/>" + percentageText(logicalPercent) + "% ext.</td>\n";
        }

        if (ebr.part_next == -1){
            break;
        }

        if (ebr.part_next <= currentPosition || ebr.part_next >= extendedEnd){
            break;
        }

        currentPosition = ebr.part_next;
    }

    return true;
}


//agrega inodo al arbol
bool ReportManager::addInodeTree(const string& diskPath, const SuperBlock& superBlock, int inodeIndex, string& dotContent) const {
    FileSystemManager fileSystemManager;
    Inode inode;

    if (!fileSystemManager.readInode(diskPath, superBlock, inodeIndex, inode)){
        return false;
    }

    dotContent += "inode" + to_string(inodeIndex) + " [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='2'><b>INODO " + to_string(inodeIndex) + "</b></td></tr>\n";
    dotContent += "<tr><td>UID</td><td>" + to_string(inode.i_uid) + "</td></tr>\n";
    dotContent += "<tr><td>GID</td><td>" + to_string(inode.i_gid) + "</td></tr>\n";
    dotContent += "<tr><td>SIZE</td><td>" + to_string(inode.i_s) + "</td></tr>\n";
    dotContent += "<tr><td>TYPE</td><td>" + string(1, inode.i_type) + "</td></tr>\n";
    dotContent += "<tr><td>PERM</td><td>" + charArrayToString(inode.i_perm, 3) + "</td></tr>\n";
    dotContent += "</table>\n";
    dotContent += ">];\n";

    return true;
}


//agrega bloque carpeta al arbol
bool ReportManager::addFolderBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent) const {
    FileSystemManager fileSystemManager;
    FolderBlock folderBlock;

    if (!fileSystemManager.readFolderBlock(diskPath, superBlock, blockIndex, folderBlock)){
        return false;
    }

    dotContent += "block" + to_string(blockIndex) + " [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='2'><b>CARPETA " + to_string(blockIndex) + "</b></td></tr>\n";

    for (int entryPos = 0; entryPos < 4; entryPos++){
        string name = escapeGraphvizText(charArrayToString(folderBlock.b_content[entryPos].b_name, 12));

        dotContent += "<tr><td>" + name + "</td><td>" + to_string(folderBlock.b_content[entryPos].b_inodo) + "</td></tr>\n";
    }

    dotContent += "</table>\n";
    dotContent += ">];\n";

    return true;
}


//agrega bloque archivo al arbol
bool ReportManager::addFileBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent) const {
    FileSystemManager fileSystemManager;
    FileBlock fileBlock;

    if (!fileSystemManager.readFileBlock(diskPath, superBlock, blockIndex, fileBlock)){
        return false;
    }

    string content = escapeGraphvizText(charArrayToString(fileBlock.b_content, 64));

    dotContent += "block" + to_string(blockIndex) + " [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td><b>ARCHIVO " + to_string(blockIndex) + "</b></td></tr>\n";
    dotContent += "<tr><td align='left'>" + content + "</td></tr>\n";
    dotContent += "</table>\n";
    dotContent += ">];\n";

    return true;
}


//agrega bloque de apuntadores al arbol
bool ReportManager::addPointerBlockTree(const string& diskPath, const SuperBlock& superBlock, int blockIndex, int level, string& dotContent) const {
    FileSystemManager fileSystemManager;
    PointerBlock pointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, blockIndex, pointerBlock)){
        return false;
    }

    dotContent += "block" + to_string(blockIndex) + " [label=<\n";
    dotContent += "<table border='1' cellborder='1' cellspacing='0'>\n";
    dotContent += "<tr><td colspan='2'><b>APUNTADORES " + to_string(blockIndex) + "</b></td></tr>\n";

    for (int pointerPos = 0; pointerPos < 16; pointerPos++){
        dotContent += "<tr><td>" + to_string(pointerPos) + "</td><td>" + to_string(pointerBlock.b_pointers[pointerPos]) + "</td></tr>\n";
    }

    dotContent += "</table>\n";
    dotContent += ">];\n";

    //solo sigue por bloques de apuntadores
    if (level > 1){
        for (int pointerPos = 0; pointerPos < 16; pointerPos++){
            int nextBlock = pointerBlock.b_pointers[pointerPos];

            if (nextBlock == -1){
                continue;
            }

            dotContent += "block" + to_string(blockIndex) + " -> block" + to_string(nextBlock) + ";\n";

            if (!addPointerBlockTree(diskPath, superBlock, nextBlock, level - 1, dotContent)){
                return false;
            }
        }
    }

    return true;
}


//convierte permisos octales a rwx
string ReportManager::permissionToText(const Inode& inode) const {
    string result;

    for (int permissionPos = 0; permissionPos < 3; permissionPos++){
        char permission = inode.i_perm[permissionPos];

        if (permission < '0' || permission > '7'){
            result += "---";
            continue;
        }

        int value = permission - '0';

        result += (value & 4) ? "r" : "-";
        result += (value & 2) ? "w" : "-";
        result += (value & 1) ? "x" : "-";
    }

    return result;
}
