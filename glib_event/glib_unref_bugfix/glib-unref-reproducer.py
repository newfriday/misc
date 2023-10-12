import os
import sys
import threading
import time

from gi.repository import GLib


def cb(*args, **kwargs):
    pass


context = GLib.MainContext.default()

pipes = []
for _ in range(2):
    pipes.append(os.pipe())

def makesource(idx):
    return GLib.io_add_watch(pipes[idx][0], GLib.IO_IN, cb)


def setsourcename(source_id, name):
    source = context.find_source_by_id(source_id)
    GLib.Source.set_name(source, name)


def runmain():
    while True:
        GLib.MainContext.default().iteration(True)


source1 = makesource(0)
source2 = makesource(1)


threading.Thread(target=runmain).start()
time.sleep(1)
print("\n\nPY: sleeping")
time.sleep(1)
print("\n\n")

print("PY: set source names")
setsourcename(source1, "source1")
setsourcename(source2, "source2")

print("PY: wakeup source1")
GLib.MainContext.default().wakeup()
time.sleep(1)
print("PY: remove source1")
GLib.source_remove(source1)
time.sleep(2)
print("PY: remove source2")
GLib.source_remove(source2)
print("PY: big sleep")
time.sleep(10)

print("PY: reached end")
sys.exit(0)
