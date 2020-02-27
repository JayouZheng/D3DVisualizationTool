//
// GeometryManager.h
//

#pragma once

#include "../AppUtil.h"
#include "DeviceResources.h"

namespace DX
{
	namespace GeometryManager
	{
		// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
		// geometries are stored in one vertex and index buffer.  It provides the offsets
		// and data needed to draw a subset of geometry stores in the vertex and index
		// buffers.
		struct SubmeshGeometry
		{
			// DrawIndexedInstanced parameters.
			uint32 IndexCountPerInstance = 0;
			uint32 InstanceCount = 1;
			uint32 StartIndexLocation = 0;
			int32  BaseVertexLocation = 0;
			uint32 StartInstanceLocation = 0;

			// Bounding box of the geometry defined by this submesh.
			DirectX::BoundingBox Bounds;
		};

		struct MeshGeometry
		{
			// Give it a name so we can look it up by name.
			std::string Name;

			// System memory copies.  Use Blobs because the vertex/index format can be generic.
			// It is up to the client to cast appropriately.
			// Using D3DCreateBlob(...), CopyMemory(...).
			ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
			ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

			// GPU Buffer.
			ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
			ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

			// Uploader Buffer...CPU to GPU.
			ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
			ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

			// Data about the buffers.
			uint32 VertexByteStride = 0;
			uint32 VertexBufferByteSize = 0;
			DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
			uint32 IndexBufferByteSize = 0;

			// A MeshGeometry may store multiple geometries in one vertex/index buffer.
			// Use this container to define the Submesh geometries so we can draw
			// the Submeshes individually.
			std::unordered_map<std::string, SubmeshGeometry> DrawArgs;	

			D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
			{
				D3D12_VERTEX_BUFFER_VIEW vbv;
				vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
				vbv.StrideInBytes = VertexByteStride;
				vbv.SizeInBytes = VertexBufferByteSize;

				return vbv;
			}

			D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
			{
				D3D12_INDEX_BUFFER_VIEW ibv;
				ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
				ibv.Format = IndexFormat;
				ibv.SizeInBytes = IndexBufferByteSize;

				return ibv;
			}

			// We can free this memory after we finish upload to the GPU.
			void DisposeUploaders()
			{
				VertexBufferUploader = nullptr;
				IndexBufferUploader = nullptr;
			}
		};

		enum RenderLayer
		{
			Opaque,
			Transparent,
			AlphaTested,
			ScreenQuad,
			Line,
			Selected,
			Count
		};

		struct RenderItem
		{
		public:

			RenderItem() { ObjectCBufferIndex = ObjectCount++; }
			RenderItem(const RenderItem& item) = delete;

			static int32 ObjectCount;
			int32 ObjectCBufferIndex; // Also as RenderItem Index.

			// Give it a name so we can look it up by name.
			std::string Name;

			std::unique_ptr<MeshGeometry> Geometry = nullptr;
			
			Matrix4 Local = Matrix4(EIdentityTag::kIdentity);
			Matrix4 World = Matrix4(EIdentityTag::kIdentity);

			bool bObjectDataChanged = true;

			// Primitive topology.
			D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			///////////////////////////////////////////////////////
			// Custom Data Field. / Per Box Structure Buffer Offset.
			int PerBoxSBufferOffset = 0;
			///////////////////////////////////////////////////////

			// NOTE: NO SubmeshGeometry.
			template<typename _T1, typename _T2>
			void CreateCommonGeometry(DeviceResources* deviceResources, const std::string& name, const std::vector< _T1>& vertices, const std::vector< _T2>& indices)
			{
				const UINT vbByteSize = (UINT)vertices.size() * sizeof(_T1);
				const UINT ibByteSize = (UINT)indices.size() * sizeof(_T2);

				Name = name;

				Geometry = std::make_unique<MeshGeometry>();

				ThrowIfFailed(D3DCreateBlob(vbByteSize, &Geometry->VertexBufferCPU));
				CopyMemory(Geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

				ThrowIfFailed(D3DCreateBlob(ibByteSize, &Geometry->IndexBufferCPU));
				CopyMemory(Geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

				deviceResources->CreateDefaultBuffer(vertices.data(), vbByteSize,
					&Geometry->VertexBufferGPU, &Geometry->VertexBufferUploader);
				deviceResources->CreateDefaultBuffer(indices.data(), ibByteSize,
					&Geometry->IndexBufferGPU, &Geometry->IndexBufferUploader);

				// Vertex Buffer View Data.
				Geometry->VertexByteStride = sizeof(_T1);
				Geometry->VertexBufferByteSize = vbByteSize;

				// Index Buffer View Data.
				if (std::is_same<_T2, uint16>::value)
					Geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
				else if ((std::is_same<_T2, uint32>::value))
					Geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
				Geometry->IndexBufferByteSize = ibByteSize;

				SubmeshGeometry submesh;
				submesh.IndexCountPerInstance = (UINT)indices.size();
				submesh.InstanceCount = 1;
				submesh.StartIndexLocation = 0;
				submesh.BaseVertexLocation = 0;
				submesh.StartInstanceLocation = 0;

				Geometry->DrawArgs[name] = submesh;
			}
							
			template<typename TLambda = PFVOID>
			void Draw(ID3D12GraphicsCommandList* commandList, const TLambda& lambda = defalut)
			{
				commandList->IASetVertexBuffers(0, 1, &Geometry->VertexBufferView());
				commandList->IASetIndexBuffer(&Geometry->IndexBufferView());
				commandList->IASetPrimitiveTopology(PrimitiveType);

				// Set/Bind Per Object Data.
				lambda();
				
				for (auto& e : Geometry->DrawArgs)
				{
					SubmeshGeometry& submesh = e.second;
					commandList->DrawIndexedInstanced(submesh.IndexCountPerInstance,
						submesh.InstanceCount,
						submesh.StartIndexLocation,
						submesh.BaseVertexLocation,
						submesh.StartInstanceLocation);
				}
			}

		private:

			static void defalut() {}
		};

		struct ColorVertex
		{
			Vector3 Pos;
			Vector4 Color;
		};

		struct Vertex
		{
			Vertex() {}
			Vertex(
				const DirectX::XMFLOAT3& p,
				const DirectX::XMFLOAT3& n,
				const DirectX::XMFLOAT3& t,
				const DirectX::XMFLOAT2& uv) :
				Position(p),
				Normal(n),
				TangentU(t),
				TexC(uv) {}
			Vertex(
				float px, float py, float pz,
				float nx, float ny, float nz,
				float tx, float ty, float tz,
				float u, float v) :
				Position(px, py, pz),
				Normal(nx, ny, nz),
				TangentU(tx, ty, tz),
				TexC(u, v) {}

			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT3 Normal;
			DirectX::XMFLOAT3 TangentU;
			DirectX::XMFLOAT2 TexC;
		};

		template<typename TVertex>
		struct MeshData
		{
			std::vector<TVertex> Vertices;
			std::vector<uint32> Indices32;

			std::vector<uint16>& GetIndices16()
			{
				if (mIndices16.empty())
				{
					mIndices16.resize(Indices32.size());
					for (size_t i = 0; i < Indices32.size(); ++i)
						mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}

				return mIndices16;
			}

		private:
			std::vector<uint16> mIndices16;
		};

		class GeometryCreator
		{
		public:

			///<summary>
			/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
			/// at the origin with the specified width and depth.
			///</summary>
			static MeshData<ColorVertex> CreateLineGrid(float width, float depth, uint32 m, uint32 n);

			static MeshData<ColorVertex> CreateDefaultBox();

			///<summary>
			/// Creates a box centered at the origin with the given dimensions, where each
			/// face has m rows and n columns of vertices.
			///</summary>
			static MeshData<Vertex> CreateBox(float width, float height, float depth, uint32 numSubdivisions);

			///<summary>
			/// Creates a sphere centered at the origin with the given radius.  The
			/// slices and stacks parameters control the degree of tessellation.
			///</summary>
			static MeshData<Vertex> CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);

			///<summary>
			/// Creates a geosphere centered at the origin with the given radius.  The
			/// depth controls the level of tessellation.
			///</summary>
			static MeshData<Vertex> CreateGeosphere(float radius, uint32 numSubdivisions);

			///<summary>
			/// Creates a cylinder parallel to the y-axis, and centered about the origin.
			/// The bottom and top radius can vary to form various cone shapes rather than true
			/// cylinders.  The slices and stacks parameters control the degree of tessellation.
			///</summary>
			static MeshData<Vertex> CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

			///<summary>
			/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
			/// at the origin with the specified width and depth.
			///</summary>
			static MeshData<Vertex> CreateGrid(float width, float depth, uint32 m, uint32 n);

			///<summary>
			/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
			///</summary>
			static MeshData<Vertex> CreateQuad(float x, float y, float w, float h, float depth);

		private:

			static void Subdivide(MeshData<Vertex>& meshData);
			static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
			static void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData<Vertex>& meshData);
			static void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData<Vertex>& meshData);
		};

	}
}

