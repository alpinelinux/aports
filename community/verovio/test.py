#!/usr/bin/python3
import verovio
import json
tk = verovio.toolkit()
print(json.dumps(tk.getAvailableOptions()), end='')
