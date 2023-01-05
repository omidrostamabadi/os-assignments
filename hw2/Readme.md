# Build and run
`make all` produces the executable `httpserver` in the same directory. Run using `./httpserver` in the current directory.

# Usage
Server can be used either for file serving or proxy mode. Running `./httpserver --help` outputs the usage information for both modes:

**File serving**: `./httpserver --files FILE_DIR/ [--port PORT --num-threads N]` <br>
All file requests are served reletaive to the **FILE_DIR** directory. The default port is 8000. The program will run in single-threaded mode if **num-threads** is not given. <br>
**Proxy**: `./httpserver --proxy ADDRESS:PORT [--port PORT --num-threads N]`
**ADDRESS** Can be either URL or IP address. Default port is 8000 and num-threads will be equal to 1 if not specified.
