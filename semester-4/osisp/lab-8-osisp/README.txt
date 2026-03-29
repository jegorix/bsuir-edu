lab-8-osisp

Build:
make
make MODE=release

Run server:
./build/debug/myserver <root_dir> <port>

Run client:
./build/debug/myclient <host> [port]

Examples:
./build/debug/myserver ./testroot 23456
./build/debug/myclient 127.0.0.1 23456
./build/debug/myclient localhost 23456

Supported commands:
ECHO <text>
INFO
LIST
CD <dir>
QUIT
@<file>

Notes:
- On connect the client receives server_info.txt.
- Each connection keeps its own current directory.
- Attempts to leave the server root with CD are ignored silently.
- LIST prints directories with '/', symlinks to regular files with '-->',
  and symlinks to symlinks with '-->>'.
