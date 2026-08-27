# Kopiert die fertige firmware.bin nach build_output/firmware/<env>.bin, damit
# die Abbilder aller Envs nebeneinander liegen und am Namen erkennbar sind
# (.pioenvs/<env>/firmware.bin heisst in jedem Env gleich).
#
# Als einziges Skript aus piotools/ ist es in platformio.ini eingebunden. Die
# uebrigen sechs (Upload- und objdump-Helfer aus dem Original-HeishaMon) sind
# in 3.16.0 entfernt worden: Sie waren nirgends referenziert, und obj-dump.py
# rief seit dem ESP32-Port eine Toolchain auf, die dieses Projekt gar nicht
# mehr installiert. Groessennachweise laufen hier ueber
# xtensa-esp32s3-elf-size und -nm auf der .elf.

Import('env')
import os
import shutil

OUTPUT_DIR = "build_output{}".format(os.path.sep)


def bin_copy(source, target, env):
    # .pioenvs/<env>/firmware.bin -> der Env-Name steht an zweiter Stelle
    variant = str(target[0]).split(os.path.sep)[1]

    # Zielverzeichnis anlegen, falls es den ersten Build noch nicht gab
    firmware_dir = "{}firmware".format(OUTPUT_DIR)
    if not os.path.isdir(firmware_dir):
        os.makedirs(firmware_dir)

    bin_file = "{}{}{}.bin".format(firmware_dir, os.path.sep, variant)

    # Vorgaenger entfernen, sonst blieb bei einem Fehlschlag der alte Stand
    # liegen und saehe wie ein frischer Build aus
    if os.path.isfile(bin_file):
        os.remove(bin_file)

    shutil.copy(str(target[0]), bin_file)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_copy])
