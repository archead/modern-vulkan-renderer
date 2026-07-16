#pragma once
#include <mikktspace.h>
#include <vector>
#include "Types.hpp"
namespace mikkutil {
	void unweldVertices(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

    int getNumFaces(const SMikkTSpaceContext *ctx);

    int getNumVerticesOfFace(const SMikkTSpaceContext *ctx, int iFace);

    void getPosition(const SMikkTSpaceContext* ctx, float out[3], int face, int vert);

    void getNormal(const SMikkTSpaceContext* ctx, float out[3], int face, int vert);

    void getTexCoord(const SMikkTSpaceContext* ctx, float out[3], int face, int vert);

    void setTSpaceBasic(const SMikkTSpaceContext* ctx, const float tangent[3], float sign, int face, int vert);

    void generateTangents(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

}