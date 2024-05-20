import subprocess
import pathlib
import os
from pathlib import Path
d = os.path.dirname(os.path.abspath(__file__))
path = Path(d)
ROOT_PATH = path.parent.parent.absolute()

CFG_PATH = f"{ROOT_PATH}/configs/DMesh12x10_TileSize256_CramSize256x256.cfg"
EXEC_PATH = f"{ROOT_PATH}/build/PIM_simulator"

OUTPUT_PATH = "output_bert"
pathlib.Path(OUTPUT_PATH).mkdir(parents=True, exist_ok=True) 



# BERT + MLPs
list=[
    'gemm_tiled_M384_K768_N768',
    'gemm_tiled_M384_K64_N384',
    'gemm_tiled_M384_K384_N64',
    'gemm_tiled_M384_K768_N3072',
    'gemm_tiled_M384_K3072_N768',
]

for task in list:
# for task in ["conv2d_low_latency_only_load_weight"]:
    print(f"Running {task}")
    subprocess.run([EXEC_PATH, "-c", CFG_PATH, "-m", task, "-l", f"{OUTPUT_PATH}/{task}.log"])
