# Sistema de Archivos Distribuido - Versión Mejorada

Este es un sistema de archivos distribuido mejorado con soporte para hilos, carpetas, procesos daemon, y configuración de IP/puerto por línea de comandos.

## Características Principales

- **Comentarios y mensajes**
- **Uso extensivo de funciones** para mejor legibilidad del código
- **Soporte para hilos (threads)** para manejo concurrente de clientes
- **Manejo de archivos y carpetas** - las carpetas se transfieren completas
- **Proceso daemon** - el servidor de archivos se ejecuta en segundo plano
- **Interfaz de comandos** - comandos interactivos para el servidor de archivos
- **Carpeta especial de descargas** - todos los archivos se guardan en `descargas_distribuidas`
- **Configuración por línea de comandos** - IP y puertos se especifican como argumentos

## Componentes del Sistema

### 1. Servidor DNS (`servidor_dns_nuevo`)
- Mantiene la tabla de elementos registrados (archivos y carpetas)
- Maneja solicitudes de ubicación de elementos
- Controla el bloqueo de archivos para escritura
- Usa hilos para manejar múltiples clientes concurrentemente

### 2. Servidor de Archivos (`servidor_archivos_nuevo`)
- Interfaz de comandos interactiva
- Proceso daemon para servir archivos
- Comandos disponibles:
  - `habilitar` - Registra el servidor en DNS e inicia daemon
  - `subir_archivo <nombre>` - Registra un archivo específico
  - `subir_carpeta <nombre>` - Registra una carpeta específica
  - `actualizar_todo` - Registra todos los archivos .txt y carpetas
  - `salir` - Termina el programa

### 3. Cliente (`cliente_nuevo`)
- Descarga archivos y carpetas del sistema distribuido
- Guarda todo en la carpeta `descargas_distribuidas`
- Preserva los nombres originales de archivos y carpetas
- Soporte para lectura y escritura de archivos

## Compilación

```bash
# Compilar todos los programas
make -f makefile_nuevo

# Crear archivos de prueba
make -f makefile_nuevo test_files

# Ver ayuda
make -f makefile_nuevo help
```

## Uso del Sistema

### 1. Iniciar el Servidor DNS

```bash
./servidor_dns_nuevo <ip> <puerto>
```

Ejemplo:
```bash
./servidor_dns_nuevo 127.0.0.1 9000
```

### 2. Iniciar Servidor(es) de Archivos

```bash
./servidor_archivos_nuevo <ip_servidor> <puerto_servidor> <ip_dns> <puerto_dns>
```

Ejemplo:
```bash
./servidor_archivos_nuevo 127.0.0.1 9001 127.0.0.1 9000
```

Una vez iniciado, usar los comandos interactivos:
- `habilitar` - Para registrar el servidor y iniciar el daemon
- `subir_archivo archivo.txt` - Para registrar archivos específicos
- `subir_carpeta mi_carpeta` - Para registrar carpetas específicas
- `actualizar_todo` - Para registrar todo el contenido disponible

### 3. Usar el Cliente

```bash
./cliente_nuevo <ip_dns> <puerto_dns> <nombre_elemento> <LEER|ESCRIBIR>
```

Ejemplos:
```bash
# Descargar un archivo para lectura
./cliente_nuevo 127.0.0.1 9000 archivo1.txt LEER

# Descargar un archivo para edición
./cliente_nuevo 127.0.0.1 9000 archivo1.txt ESCRIBIR

# Descargar una carpeta completa
./cliente_nuevo 127.0.0.1 9000 carpeta_prueba LEER
```

## Flujo de Trabajo Típico

1. **Preparar el entorno:**
   ```bash
   make -f makefile_nuevo
   make -f makefile_nuevo test_files
   ```

2. **Iniciar el DNS en una terminal:**
   ```bash
   ./servidor_dns_nuevo 127.0.0.1 9000
   ```

3. **Iniciar un servidor de archivos en otra terminal:**
   ```bash
   ./servidor_archivos_nuevo 127.0.0.1 9001 127.0.0.1 9000
   ```
   
   Luego escribir: `habilitar`

4. **Usar el cliente en otra terminal:**
   ```bash
   ./cliente_nuevo 127.0.0.1 9000 archivo1.txt LEER
   ```

5. **Verificar los archivos descargados:**
   ```bash
   ls descargas_distribuidas/
   ```

## Arquitectura del Sistema

### Comunicación
- **DNS ↔ Servidor de Archivos**: Registro y gestión de elementos
- **DNS ↔ Cliente**: Resolución de ubicación de elementos
- **Servidor de Archivos ↔ Cliente**: Transferencia de archivos/carpetas

### Protocolos de Mensajes

#### DNS Server
- `REGISTRAR_ELEMENTO <nombre> <ip> <puerto> <ARCHIVO|CARPETA>`
- `SOLICITAR_ELEMENTO <nombre> <LEER|ESCRIBIR>`
- `DESBLOQUEAR_ELEMENTO <nombre>`

#### File Server Daemon
- `OBTENER_ELEMENTO <nombre>`
- `ACTUALIZAR_ARCHIVO <nombre>`

### Manejo de Carpetas

Cuando se solicita una carpeta, el sistema:
1. Crea la carpeta de destino en `descargas_distribuidas/`
2. Transfiere todos los archivos de la carpeta
3. Recrea la estructura de subcarpetas
4. Preserva los nombres originales

### Bloqueo de Archivos

- Los archivos se bloquean automáticamente para operaciones de escritura
- Solo un cliente puede escribir un archivo a la vez
- Los bloqueos se liberan automáticamente después de la escritura

## Limpieza

```bash
# Limpiar archivos compilados y descargas
make -f makefile_nuevo clean
```

## Notas Técnicas

- El sistema usa sockets TCP para todas las comunicaciones
- Los hilos se crean dinámicamente para cada cliente
- Los procesos daemon se ejecutan en segundo plano
- Se preserva la compatibilidad con archivos .txt del sistema original
- La carpeta `descargas_distribuidas` se crea automáticamente

## Diferencias con el Sistema Original

1. **Comentarios en español** en lugar de inglés
2. **Soporte para carpetas** además de archivos
3. **Proceso daemon** en lugar de bucle principal bloqueante
4. **Interfaz de comandos** para el servidor de archivos
5. **Uso de hilos** para manejo concurrente
6. **Configuración por argumentos** en lugar de valores hardcodeados
7. **Carpeta especial** para todas las descargas
8. **Preservación de nombres** originales de archivos y carpetas 