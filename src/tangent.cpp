#include "tangent.h"

#include "mikktspace.h"
#include "model.h"

int get_num_faces(const SMikkTSpaceContext* context) {
    const auto* mesh = static_cast<Primitive*>(context->m_pUserData);
    return static_cast<int>(mesh->indices.size() / 3);
}

int get_num_vertices_of_face(const SMikkTSpaceContext* _context, const int _i_face) {
    return 3;
}

void get_position(const SMikkTSpaceContext* context, float pos_out[], const int i_face, const int i_vert) {
    const auto* mesh = static_cast<Primitive*>(context->m_pUserData);
    const uint32_t index = mesh->indices[i_face * 3 + i_vert];
    const auto& pos = mesh->vertices[index].position;

    pos_out[0] = pos.x;
    pos_out[1] = pos.y;
    pos_out[2] = pos.z;
}

void get_normal(const SMikkTSpaceContext* pContext, float norm_out[], const int i_face, const int i_vert) {
    const auto* mesh = static_cast<Primitive*>(pContext->m_pUserData);
    const uint32_t index = mesh->indices[i_face * 3 + i_vert];
    const auto& norm = mesh->vertices[index].normal;

    norm_out[0] = norm.x;
    norm_out[1] = norm.y;
    norm_out[2] = norm.z;
}

void get_texcoord(const SMikkTSpaceContext* pContext, float texcoord_out[], const int i_face, const int i_vert) {
    const auto* mesh = static_cast<Primitive*>(pContext->m_pUserData);
    const uint32_t index = mesh->indices[i_face * 3 + i_vert];
    const auto& uv = mesh->vertices[index].texcoord;

    texcoord_out[0] = uv.x;
    texcoord_out[1] = uv.y;
}

void set_tspace_basic(const SMikkTSpaceContext* context, const float tangent[], const float sign, const int i_face,
                      const int i_vert) {
    auto* mesh = static_cast<Primitive*>(context->m_pUserData);
    const uint32_t idx = mesh->indices[i_face * 3 + i_vert];

    mesh->vertices[idx].tangent.x = tangent[0];
    mesh->vertices[idx].tangent.y = tangent[1];
    mesh->vertices[idx].tangent.z = tangent[2];
    mesh->vertices[idx].tangent.w = sign * -1.0f;
}

void TangentGenerator::generate(Primitive* primitive) {
    SMikkTSpaceInterface interface{
        .m_getNumFaces = get_num_faces,
        .m_getNumVerticesOfFace = get_num_vertices_of_face,
        .m_getPosition = get_position,
        .m_getNormal = get_normal,
        .m_getTexCoord = get_texcoord,
        .m_setTSpaceBasic = set_tspace_basic,
    };

    const SMikkTSpaceContext context{
        .m_pInterface = &interface,
        .m_pUserData = primitive
    };

    genTangSpaceDefault(&context);
}