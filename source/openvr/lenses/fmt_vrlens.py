from inc_noesis import *

#this is a noesis script to read/write VR lens files

def registerNoesisTypes():
	handle = noesis.register("VR Lens Model", ".vrlens")
	noesis.setHandlerTypeCheck(handle, vrlensCheckType)
	noesis.setHandlerLoadModel(handle, vrlensLoadModel)
	noesis.setHandlerWriteModel(handle, vrlensWriteModel)
	return 1

VRLENS_HEADER = 0x539474
VRLENS_VERSION = 0x1010
VERTEX_SIZE = 24

def vrlensCheckType(data):
	if len(data) < 8:
		return 0
	id, ver = noeUnpack("<II", data[:8])
	if id != VRLENS_HEADER or ver != VRLENS_VERSION:
		return 0
	return 1

def vrlensLoadModel(data, mdlList):
	bs = NoeBitStream(data)
	bs.readInt()
	bs.readInt()
	vertCount = bs.readInt()
	indexCount = bs.readInt()
	
	vertexData = data[bs.tell():]

	ctx = rapi.rpgCreateContext()
	rapi.rpgBindPositionBufferOfs(vertexData, noesis.RPGEODATA_FLOAT, VERTEX_SIZE, 0)
	rapi.rpgBindUV1BufferOfs(vertexData, noesis.RPGEODATA_FLOAT, VERTEX_SIZE, 12)
	rapi.rpgCommitTriangles(vertexData[vertCount * VERTEX_SIZE:], noesis.RPGEODATA_INT, indexCount, noesis.RPGEO_TRIANGLE, 1)
	rapi.rpgOptimize()
	mdl = rapi.rpgConstructModel()
	mdlList.append(mdl)

	return 1

def vrlensWriteModel(mdl, bs):
	bs.writeInt(VRLENS_HEADER)
	bs.writeInt(VRLENS_VERSION)

	mesh = mdl.meshes[0]
	bs.writeInt(len(mesh.positions))
	bs.writeInt(len(mesh.indices))
	for vcmpIndex in range(len(mesh.positions)):
		bs.writeBytes(mesh.positions[vcmpIndex].toBytes())
		bs.writeBytes(mesh.uvs[vcmpIndex].toBytes())
	for idx in mesh.indices:
		bs.writeInt(idx)
	return 1
