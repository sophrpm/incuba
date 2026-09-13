# MIA_2S2026_Practica_202405047
Práctica Única MIA S2 2026

# Eliminar la carpeta build anterior
rm -rf backend/build

# Generar los archivos de compilación con CMake
cmake -S backend -B backend/build

# Compilar el proyecto
cmake --build backend/build

# Ejecutar el backend
./backend/build/202405047_ext2_backend

# Entrar al frontend
cd frontend

# Instalar dependencias si es la primera vez
npm install

# Ejecutar el frontend
npm run dev