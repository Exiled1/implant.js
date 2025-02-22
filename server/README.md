# implant.js - Server

## Usage

The server only depends on the standard Python library. The default port is 1337, but can be overridden via `-p`:

```
$ ./server.py -p 31337
```

By defaults, loads modules from the `/modules` directory in the repo root. You can override this by setting `$IMPJS_MODULE_DIR` to a different directory.

## Tests

Install `pytest`, and then you can run the test suite:

```
$ python -m pytest tests
```