#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define JSON_NOEXCEPTION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_IMPLEMENTATION
#include "libraries/tiny_gltf.h"

void loadMesh(tinygltf::Model &model, tinygltf::Mesh &mesh)
{
	for(auto &primitive : mesh.primitives)
	{
		for(auto &attribute : primitive.attributes)
		{
			auto &accessor = model.accessors[attribute.second];
			auto &view = model.bufferViews[accessor.bufferView];
			auto &buffer = model.buffers[view.buffer];

			auto count = accessor.count;
			auto offset = view.byteOffset + accessor.byteOffset;
			auto stride = accessor.ByteStride(view);

			if(attribute.first.compare("POSITION") == 0 && accessor.type == TINYGLTF_TYPE_VEC3
			 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				for(size_t i = offset; i < offset + count * stride; i += stride)
				{
					float *p = (float*)(&buffer.data.at(0) + i);
					Vertex vertex = {{
						{p[0], p[1], -0.5f - p[2]},
						{1.0f, 1.0f, 1.0f},
						{0.0f, 0.0f}
					}};
					vertices[vertexCount++] = vertex;
				}
			}
		}

		if(primitive.indices > -1)
		{
			auto &accessor = model.accessors[primitive.indices];
			auto &view = model.bufferViews[accessor.bufferView];
			auto &buffer = model.buffers[view.buffer];

			auto count = accessor.count;
			auto offset = view.byteOffset + accessor.byteOffset;
			auto stride = accessor.ByteStride(view);

			if(primitive.mode == TINYGLTF_MODE_TRIANGLES && accessor.type == TINYGLTF_TYPE_SCALAR
			 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				for(size_t i = offset; i < offset + count * stride; i += stride)
				{
					unsigned int *p = (unsigned int*)(&buffer.data.at(0) + i);
					indices[indexCount++] = *p;
				}
			}
		}
	}
}

void loadNode(tinygltf::Model &model, tinygltf::Node &node)
{
	if(node.mesh != -1)
		loadMesh(model, model.meshes[node.mesh]);
	for(size_t i=0; i<node.children.size(); i++)
		loadNode(model, model.nodes[node.children[i]]);
}

void loadModel(const char* filename)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;
	loader.LoadASCIIFromFile(&model, &err, &warn, filename);

	auto &scene = model.scenes[model.defaultScene];
	for(size_t i=0; i<scene.nodes.size(); i++)
		loadNode(model, model.nodes[scene.nodes[i]]);
}
