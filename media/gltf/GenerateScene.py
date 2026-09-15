"""Generate the original Ldx12 glTF fixture; no third-party assets or packages."""
import base64
import copy
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parent
blob = bytearray()
views = []
accessors = []


def accessor(values, fmt, kind, component, bounds=False):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    for value in values:
        blob.extend(struct.pack('<' + fmt, *value))
    views.append(dict(buffer=0, byteOffset=offset, byteLength=len(blob)-offset))
    item = dict(bufferView=len(views)-1, componentType=component,
                count=len(values), type=kind)
    if bounds:
        item['min'] = list(map(min, zip(*values)))
        item['max'] = list(map(max, zip(*values)))
    accessors.append(item)
    return len(accessors)-1


corners = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
apex = (0, 2, 0)
pyramid = []
for i in range(4):
    pyramid.extend([corners[i], apex, corners[(i+1) % 4]])
pyramid.extend([corners[0], corners[1], corners[2], corners[0], corners[2], corners[3]])
positions = accessor(pyramid, '3f', 'VEC3', 5126, True)
floor = accessor([(-3, 0, -3), (3, 0, -3), (3, 0, 3), (-3, 0, 3)], '3f', 'VEC3', 5126, True)
normals = accessor([(0, 1, 0)] * 4, '3f', 'VEC3', 5126)
indices = accessor([(i,) for i in [0, 2, 1, 0, 3, 2]], 'H', 'SCALAR', 5123)
scene = dict(
    asset=dict(version='2.0', generator='Ldx12 GenerateScene.py'), scene=0,
    scenes=[dict(nodes=[0, 3])],
    nodes=[dict(name='Parent', translation=[0, 1, 0], children=[1, 2]),
           dict(name='Cyan pyramid', mesh=0, translation=[-1.5, 0, 0]),
           dict(name='Mirrored coral pyramid', mesh=1, translation=[1.5, 0, 0], scale=[0.75, 0.6, -0.75]),
           dict(name='Floor', mesh=2, matrix=[1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])],
    meshes=[dict(primitives=[dict(attributes=dict(POSITION=positions), material=i, mode=4)]) for i in range(2)] +
           [dict(primitives=[dict(attributes=dict(POSITION=floor, NORMAL=normals), indices=indices, material=2, mode=4)])],
    materials=[dict(pbrMetallicRoughness=dict(baseColorFactor=color, metallicFactor=0, roughnessFactor=1))
               for color in [[0.05, 0.65, 0.75, 1], [0.9, 0.2, 0.12, 1], [0.18, 0.22, 0.28, 1]]],
    buffers=[dict(byteLength=len(blob), uri='Scene.bin')], bufferViews=views, accessors=accessors)
(root / 'Scene.bin').write_bytes(blob)
(root / 'Scene.gltf').write_text(json.dumps(scene, indent=2) + '\n', encoding='utf-8')
embedded = copy.deepcopy(scene)
embedded['buffers'][0]['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(blob).decode()
(root / 'SceneEmbedded.gltf').write_text(json.dumps(embedded, indent=2) + '\n', encoding='utf-8')
del embedded['buffers'][0]['uri']
json_chunk = json.dumps(embedded, separators=(',', ':')).encode()
json_chunk += b' ' * (-len(json_chunk) % 4)
binary_chunk = bytes(blob) + b'\0' * (-len(blob) % 4)
glb = struct.pack('<III', 0x46546C67, 2, 28 + len(json_chunk) + len(binary_chunk))
glb += struct.pack('<II', len(json_chunk), 0x4E4F534A) + json_chunk
glb += struct.pack('<II', len(binary_chunk), 0x004E4942) + binary_chunk
(root / 'Scene.glb').write_bytes(glb)
