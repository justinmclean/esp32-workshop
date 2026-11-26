# PlatformIO extra script: adds `pio run -t uploadmodel`, which writes
# model/model.bin to the 'model' flash partition using the same esptool and
# port detection as a normal upload. Works on Windows, macOS and Linux.
Import("env")  # noqa: F821  (provided by PlatformIO)

import csv
import os


def model_offset(project_dir):
    with open(os.path.join(project_dir, "partitions.csv")) as f:
        for row in csv.reader(f):
            if row and row[0].strip() == "model":
                return row[3].strip()
    raise RuntimeError("no 'model' partition in partitions.csv")


def upload_model(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    model = os.path.join(project_dir, "model", "model.bin")
    if not os.path.isfile(model):
        print("model/model.bin not found. Copy it from the workshop USB stick or SD card.")
        env.Exit(1)
    env.AutodetectUploadPort()
    cmd = '"$UPLOADER" --chip esp32s3 --port "$UPLOAD_PORT" --baud $UPLOAD_SPEED write-flash %s "%s"' % (
        model_offset(project_dir), model)
    env.Exit(env.Execute(env.VerboseAction(cmd, "Writing model to flash")))


env.AddCustomTarget(  # noqa: F821
    name="uploadmodel",
    dependencies=None,
    actions=upload_model,
    title="Upload model",
    description="Write model/model.bin to the model flash partition",
)
