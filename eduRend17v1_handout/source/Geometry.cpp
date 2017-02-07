
#include "Geometry.h"


void Geometry_t::MapMatrixBuffers(
	ID3D11Buffer* matrix_buffer,
	mat4f ModelToWorldMatrix,
	mat4f WorldToViewMatrix,
	mat4f ProjectionMatrix)
{
	// Map the resource buffer, obtain a pointer and then write our matrices to it
	D3D11_MAPPED_SUBRESOURCE resource;
	dxdevice_context->Map(matrix_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	MatrixBuffer_t* matrix_buffer_ = (MatrixBuffer_t*)resource.pData;
	matrix_buffer_->ModelToWorldMatrix = ModelToWorldMatrix;
	matrix_buffer_->WorldToViewMatrix = WorldToViewMatrix;
	matrix_buffer_->ProjectionMatrix = ProjectionMatrix;
	dxdevice_context->Unmap(matrix_buffer, 0);
}


Quad_t::Quad_t(
	ID3D11Device* dxdevice,
	ID3D11DeviceContext* dxdevice_context)
	: Geometry_t(dxdevice, dxdevice_context)
{
	// Populate the vertex array with 4 vertices
	vertex_t v0, v1, v2, v3;
	v0.Pos = { -0.5, -0.5f, 0.0f };
	v0.Normal = { 0, 0, 1 };
	v0.TexCoord = { 0, 0 };
	v1.Pos = { 0.5, -0.5f, 0.0f };
	v1.Normal = { 0, 0, 1 };
	v1.TexCoord = { 0, 1 };
	v2.Pos = { 0.5, 0.5f, 0.0f };
	v2.Normal = { 0, 0, 1 };
	v2.TexCoord = { 1, 1 };
	v3.Pos = { -0.5, 0.5f, 0.0f };
	v3.Normal = { 0, 0, 1 };
	v3.TexCoord = { 1, 0 };
	vertices.push_back(v0);
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);

	// Populate the index array with two triangles
	// Triangle #1
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(3);
	// Triangle #2
	indices.push_back(1);
	indices.push_back(2);
	indices.push_back(3);

	// Vertex array descriptor
	D3D11_BUFFER_DESC vbufferDesc = { 0.0f };
	vbufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbufferDesc.CPUAccessFlags = 0;
	vbufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vbufferDesc.MiscFlags = 0;
	vbufferDesc.ByteWidth = vertices.size()*sizeof(vertex_t);
	// Data resource
	D3D11_SUBRESOURCE_DATA vdata;
	vdata.pSysMem = &vertices[0];
	// Create vertex buffer on device using descriptor & data
	HRESULT vhr = dxdevice->CreateBuffer(&vbufferDesc, &vdata, &vertex_buffer);

	//  Index array descriptor
	D3D11_BUFFER_DESC ibufferDesc = { 0.0f };
	ibufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibufferDesc.CPUAccessFlags = 0;
	ibufferDesc.Usage = D3D11_USAGE_DEFAULT;
	ibufferDesc.MiscFlags = 0;
	ibufferDesc.ByteWidth = indices.size()*sizeof(unsigned);
	// Data resource
	D3D11_SUBRESOURCE_DATA idata;
	idata.pSysMem = &indices[0];
	// Create index buffer on device using descriptor & data
	HRESULT ihr = dxdevice->CreateBuffer(&ibufferDesc, &idata, &index_buffer);

	// Local data is now loaded to device so it can be released
	vertices.clear();
	nbr_indices = indices.size();
	indices.clear();
}


void Quad_t::render() const
{
	//set topology
	dxdevice_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind our vertex buffer
	UINT32 stride = sizeof(vertex_t); //  sizeof(float) * 8;
	UINT32 offset = 0;
	dxdevice_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

	// bind our index buffer
	dxdevice_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);

	// make the drawcall
	dxdevice_context->DrawIndexed(nbr_indices, 0, 0);
}


OBJModel_t::OBJModel_t(
	const std::string& objfile,
	ID3D11Device* dxdevice,
	ID3D11DeviceContext* dxdevice_context)
	: Geometry_t(dxdevice, dxdevice_context)
{
	// Load the OBJ
	mesh_t* mesh = new mesh_t();
	mesh->load_obj(objfile);

	// Load and organize indices in ranges per drawcall (material)

	std::vector<unsigned> indices;
	size_t i_ofs = 0;

	for (auto& dc : mesh->drawcalls)
	{
		// Append the drawcall indices
		for (auto& tri : dc.tris)
			indices.insert(indices.end(), tri.vi, tri.vi + 3);

		// Create a range
		size_t i_size = dc.tris.size() * 3;
		int mtl_index = dc.mtl_index > -1 ? dc.mtl_index : -1;
		index_ranges.push_back({ i_ofs, i_size, 0, mtl_index });

		i_ofs = indices.size();
	}

	// Vertex array descriptor
	D3D11_BUFFER_DESC vbufferDesc = { 0.0f };
	vbufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbufferDesc.CPUAccessFlags = 0;
	vbufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vbufferDesc.MiscFlags = 0;
	vbufferDesc.ByteWidth = mesh->vertices.size()*sizeof(vertex_t);
	// Data resource
	D3D11_SUBRESOURCE_DATA vdata;
	vdata.pSysMem = &(mesh->vertices)[0];
	// Create vertex buffer on device using descriptor & data
	HRESULT vhr = dxdevice->CreateBuffer(&vbufferDesc, &vdata, &vertex_buffer);

	// Index array descriptor
	D3D11_BUFFER_DESC ibufferDesc = { 0.0f };
	ibufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibufferDesc.CPUAccessFlags = 0;
	ibufferDesc.Usage = D3D11_USAGE_DEFAULT;
	ibufferDesc.MiscFlags = 0;
	ibufferDesc.ByteWidth = indices.size()*sizeof(unsigned);
	// Data resource
	D3D11_SUBRESOURCE_DATA idata;
	idata.pSysMem = &indices[0];
	// Create index buffer on device using descriptor & data
	HRESULT ihr = dxdevice->CreateBuffer(&ibufferDesc, &idata, &index_buffer);

	// Copy materials from mesh
	append_materials(mesh->materials);

	// Go through materials and load textures (if any) to device

	for (auto& mtl : materials)
	{
		HRESULT hr;
		std::wstring wstr; // for conversion from string to wstring

		// map_Kd (diffuse texture)
		//
		if (mtl.map_Kd.size()) {
			// Convert the file path string to wstring
			wstr = std::wstring(mtl.map_Kd.begin(), mtl.map_Kd.end());
			// Load texture to device and obtain pointers to it
			hr = DirectX::CreateWICTextureFromFile(dxdevice, dxdevice_context, wstr.c_str(), &mtl.map_Kd_Tex, &mtl.map_Kd_TexSRV);
			// Say how it went
			printf("loading texture %s - %s\n", mtl.map_Kd.c_str(), SUCCEEDED(hr) ? "OK" : "FAILED");
		}

		// Same thing with other textres here such as mtl.map_bump (Bump/Normal texture) etc
		//
		// ...
	}

	SAFE_DELETE(mesh);
}


void OBJModel_t::render() const
{
	// Set topology
	dxdevice_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Bind vertex buffer
	UINT32 stride = sizeof(vertex_t);
	UINT32 offset = 0;
	dxdevice_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

	// Bind index buffer
	dxdevice_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);

	// Iterate drawcalls
	for (auto& irange : index_ranges)
	{
		// Fetch material
		const material_t& mtl = materials[irange.mtl_index];

		// Bind textures
		dxdevice_context->PSSetShaderResources(0, 1, &mtl.map_Kd_TexSRV);
		// ...other textures here (see material_t)

		// Make the drawcall
		dxdevice_context->DrawIndexed(irange.size, irange.start, 0);
	}
}

Cube::Cube(
	ID3D11Device* dxdevice,
	ID3D11DeviceContext* dxdevice_context)
	: Geometry_t(dxdevice, dxdevice_context)
{
	// Populate the vertex array with 4 vertices
	vertex_t v0, v1, v2, v3, v4, v5, v6,
		v7, v8, v9, v10, v11, v12,
		v13, v14, v15, v16, v17, v18,
		v19, v20, v21, v22, v23;
	//front
	v0.Pos = { -0.5, -0.5f, 0.5f };
	v0.Normal = { 0, 0, 1 };
	v0.TexCoord = { 0, 0 };
	v1.Pos = { 0.5, -0.5f, 0.5f };
	v1.Normal = { 0, 0, 1 };
	v1.TexCoord = { 0, 1 };
	v2.Pos = { 0.5, 0.5f, 0.5f };
	v2.Normal = { 0, 0, 1 };
	v2.TexCoord = { 1, 1 };
	v3.Pos = { -0.5, 0.5f, 0.5f };
	v3.Normal = { 0, 0, 1 };
	v3.TexCoord = { 1, 0 };
	//back
	v4.Pos = { -0.5, -0.5f, -0.5f };
	v4.Normal = { 0, 0, -1 };
	v4.TexCoord = { 0, 0 };
	v5.Pos = { 0.5, -0.5f, -0.5f };
	v5.Normal = { 0, 0, -1 };
	v5.TexCoord = { 0, 1 };
	v6.Pos = { 0.5, 0.5f, -0.5f };
	v6.Normal = { 0, 0, -1 };
	v6.TexCoord = { 1, 1 };
	v7.Pos = { -0.5, 0.5f, -0.5f };
	v7.Normal = { 0, 0, -1 };
	v7.TexCoord = { 1, 0 };
	//right side
	v8.Pos = { 0.5, -0.5f, 0.5f };
	v8.Normal = { 1, 0, 0 };
	v8.TexCoord = { 0, 0 };
	v9.Pos = { 0.5, -0.5f, -0.5f };
	v9.Normal = { 1, 0, 0 };
	v9.TexCoord = { 0, 1 };
	v10.Pos = { 0.5, 0.5f, -0.5f };
	v10.Normal = { 1, 0, 0 };
	v10.TexCoord = { 1, 1 };
	v11.Pos = { 0.5, 0.5f, 0.5f };
	v11.Normal = { 1, 0, 0 };
	v11.TexCoord = { 1, 0 };
	//left side
	v12.Pos = { -0.5, -0.5f, -0.5f };
	v12.Normal = { -1, 0, 0 };
	v12.TexCoord = { 0, 0 };
	v13.Pos = { -0.5, -0.5f, 0.5f };
	v13.Normal = { -1, 0, 0 };
	v13.TexCoord = { 0, 1 };
	v14.Pos = { -0.5, 0.5f, 0.5f };
	v14.Normal = { -1, 0, 0 };
	v14.TexCoord = { 1, 1 };
	v15.Pos = { -0.5, 0.5f, -0.5f };
	v15.Normal = { -1, 0, 0 };
	v15.TexCoord = { 1, 0 };
	//Upside
	v16.Pos = { -0.5, 0.5f, 0.5f };
	v16.Normal = { 0, 1, 0 };
	v16.TexCoord = { 0, 0 };
	v17.Pos = { 0.5, 0.5f, 0.5f };
	v17.Normal = { 0, 1, 0 };
	v17.TexCoord = { 0, 1 };
	v18.Pos = { 0.5, 0.5f, -0.5f };
	v18.Normal = { 0, 1, 0 };
	v18.TexCoord = { 1, 1 };
	v19.Pos = { -0.5, 0.5f, -0.5f };
	v19.Normal = { 0, 1, 0 };
	v19.TexCoord = { 1, 0 };
	//downside
	v20.Pos = { -0.5, -0.5f, 0.5f };
	v20.Normal = { 0, -1, 0 };
	v20.TexCoord = { 0, 0 };
	v21.Pos = { 0.5, -0.5f, 0.5f };
	v21.Normal = { 0, -1, 0 };
	v21.TexCoord = { 0, 1 };
	v22.Pos = { 0.5, -0.5f, -0.5f };
	v22.Normal = { 0, -1, 0 };
	v22.TexCoord = { 1, 1 };
	v23.Pos = { -0.5, -0.5f, -0.5f };
	v23.Normal = { 0, -1, 0 };
	v23.TexCoord = { 1, 0 };

	vertices.push_back(v0);
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v4);
	vertices.push_back(v5);
	vertices.push_back(v6);
	vertices.push_back(v7);
	vertices.push_back(v8);
	vertices.push_back(v9);
	vertices.push_back(v10);
	vertices.push_back(v11);
	vertices.push_back(v12);
	vertices.push_back(v13);
	vertices.push_back(v14);
	vertices.push_back(v15);
	vertices.push_back(v16);
	vertices.push_back(v17);
	vertices.push_back(v18);
	vertices.push_back(v19);
	vertices.push_back(v20);
	vertices.push_back(v21);
	vertices.push_back(v22);
	vertices.push_back(v23);


	// Populate the index array with two triangles
	// Triangle #1
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(3);
	// Triangle #2
	indices.push_back(1);
	indices.push_back(2);
	indices.push_back(3);
	// Triangle #3
	indices.push_back(7);
	indices.push_back(5);
	indices.push_back(4);
	// Triangle #4
	indices.push_back(7);
	indices.push_back(6);
	indices.push_back(5);
	// Triangle #5
	indices.push_back(8);
	indices.push_back(9);
	indices.push_back(11);
	// Triangle #6
	indices.push_back(9);
	indices.push_back(10);
	indices.push_back(11);
	//Triangle #7
	indices.push_back(12);
	indices.push_back(13);
	indices.push_back(15);
	// Triangle #8
	indices.push_back(13);
	indices.push_back(14);
	indices.push_back(15);
	// Triangle #9
	indices.push_back(16);
	indices.push_back(17);
	indices.push_back(19);
	// Triangle #10
	indices.push_back(17);
	indices.push_back(18);
	indices.push_back(19);
	// Triangle #11
	indices.push_back(23);
	indices.push_back(21);
	indices.push_back(20);
	// Triangle #12
	indices.push_back(23);
	indices.push_back(22);
	indices.push_back(21);

	// Vertex array descriptor
	D3D11_BUFFER_DESC vbufferDesc = { 0.0f };
	vbufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbufferDesc.CPUAccessFlags = 0;
	vbufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vbufferDesc.MiscFlags = 0;
	vbufferDesc.ByteWidth = vertices.size() * sizeof(vertex_t);
	// Data resource
	D3D11_SUBRESOURCE_DATA vdata;
	vdata.pSysMem = &vertices[0];
	// Create vertex buffer on device using descriptor & data
	HRESULT vhr = dxdevice->CreateBuffer(&vbufferDesc, &vdata, &vertex_buffer);

	//  Index array descriptor
	D3D11_BUFFER_DESC ibufferDesc = { 0.0f };
	ibufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibufferDesc.CPUAccessFlags = 0;
	ibufferDesc.Usage = D3D11_USAGE_DEFAULT;
	ibufferDesc.MiscFlags = 0;
	ibufferDesc.ByteWidth = indices.size() * sizeof(unsigned);
	// Data resource
	D3D11_SUBRESOURCE_DATA idata;
	idata.pSysMem = &indices[0];
	// Create index buffer on device using descriptor & data
	HRESULT ihr = dxdevice->CreateBuffer(&ibufferDesc, &idata, &index_buffer);

	// Local data is now loaded to device so it can be released
	vertices.clear();
	nbr_indices = indices.size();
	indices.clear();
}

void Cube::render() const
{
	//set topology
	dxdevice_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind our vertex buffer
	UINT32 stride = sizeof(vertex_t); //  sizeof(float) * 8;
	UINT32 offset = 0;
	dxdevice_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

	// bind our index buffer
	dxdevice_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);

	// make the drawcall
	dxdevice_context->DrawIndexed(nbr_indices, 0, 0);
}