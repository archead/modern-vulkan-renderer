#include "MikkUtil.hpp"

#include <numeric>

int mikkutil::getNumFaces(const SMikkTSpaceContext* ctx) {
    auto* data = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
    return data->indices->size() / 3;
}

int mikkutil::getNumVerticesOfFace(const SMikkTSpaceContext* ctx, int iFace) {
    return 3;
}

void mikkutil::getPosition(const SMikkTSpaceContext* ctx, float out[3], int face, int vert) {
    auto* data = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
    uint32_t i = (*data->indices)[face * 3 + vert];
    glm::vec3 p = (*data->vertices)[i].pos;
    out[0] = p.x; out[1] = p.y; out[2] = p.z;
}

void mikkutil::getNormal(const SMikkTSpaceContext* ctx, float out[3], int face, int vert) {
    auto* data = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
    uint32_t i = (*data->indices)[face * 3 + vert];
    glm::vec3 n = (*data->vertices)[i].normal;
    out[0] = n.x; out[1] = n.y; out[2] = n.z;
}

void mikkutil::getTexCoord(const SMikkTSpaceContext* ctx, float out[2], int face, int vert) {
    auto* data = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
    uint32_t i = (*data->indices)[face * 3 + vert];
    glm::vec2 uv = (*data->vertices)[i].texCoord;
    out[0] = uv.x; out[1] = uv.y;
}

void mikkutil::setTSpaceBasic(const SMikkTSpaceContext* ctx, const float tangent[3], float sign, int face, int vert) {
    auto* data = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
    uint32_t i = (*data->indices)[face * 3 + vert];
    (*data->vertices)[i].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

void mikkutil::generateTangents(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices){
    MikkTSpaceUserData userData{&vertices, &indices};
    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = getNumFaces;
    iface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    iface.m_getPosition = getPosition;
    iface.m_getNormal = getNormal;
    iface.m_getTexCoord = getTexCoord;
    iface.m_setTSpaceBasic = setTSpaceBasic;

    SMikkTSpaceContext ctx{};
    ctx.m_pInterface = &iface;
    ctx.m_pUserData = &userData;

    genTangSpaceDefault(&ctx);
}

// undeduplicates vertices and sets index array to just sequential indices used with MikkTSpace
void mikkutil::unweldVertices(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    std::vector<Vertex> newVertices;
    newVertices.reserve(indices.size());
    for (auto index : indices) {
        newVertices.push_back(vertices[index]);
    }
    vertices = std::move(newVertices);

    indices.resize(vertices.size());
    std::iota(indices.begin(), indices.end(), 0);
}
