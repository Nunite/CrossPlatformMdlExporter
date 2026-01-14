#pragma once

#include <cstdint>

struct MdlVec3
{
    float x{};
    float y{};
    float z{};
};

struct StudioHdr
{
    int32_t id{};
    int32_t version{};
    char name[64]{};
    int32_t length{};

    MdlVec3 eyeposition{};
    MdlVec3 min{};
    MdlVec3 max{};

    MdlVec3 bbmin{};
    MdlVec3 bbmax{};

    int32_t flags{};

    int32_t numbones{};
    int32_t boneindex{};

    int32_t numbonecontrollers{};
    int32_t bonecontrollerindex{};

    int32_t numhitboxes{};
    int32_t hitboxindex{};

    int32_t numseq{};
    int32_t seqindex{};

    int32_t numseqgroups{};
    int32_t seqgroupindex{};

    int32_t numtextures{};
    int32_t textureindex{};
    int32_t texturedataindex{};

    int32_t numskinref{};
    int32_t numskinfamilies{};
    int32_t skinindex{};

    int32_t numbodyparts{};
    int32_t bodypartindex{};

    int32_t numattachments{};
    int32_t attachmentindex{};

    int32_t soundtable{};
    int32_t soundindex{};
    int32_t soundgroups{};
    int32_t soundgroupindex{};

    int32_t numtransitions{};
    int32_t transitionindex{};
};

static_assert(sizeof(StudioHdr) == 244, "StudioHdr size mismatch");

struct StudioSeqHdr
{
    int32_t id{};
    int32_t version{};
    char name[64]{};
    int32_t length{};
};

static_assert(sizeof(StudioSeqHdr) == 76, "StudioSeqHdr size mismatch");

struct MStudioBodyParts
{
    char name[64]{};
    int32_t nummodels{};
    int32_t base{};
    int32_t modelindex{};
};

static_assert(sizeof(MStudioBodyParts) == 76, "MStudioBodyParts size mismatch");

struct MStudioTexture
{
    char name[64]{};
    int32_t flags{};
    int32_t width{};
    int32_t height{};
    int32_t index{};
};

static_assert(sizeof(MStudioTexture) == 80, "MStudioTexture size mismatch");

struct MStudioModel
{
    char name[64]{};
    int32_t type{};
    float boundingradius{};
    int32_t nummesh{};
    int32_t meshindex{};

    int32_t numverts{};
    int32_t vertinfoindex{};
    int32_t vertindex{};
    int32_t numnorms{};
    int32_t norminfoindex{};
    int32_t normindex{};

    int32_t numgroups{};
    int32_t groupindex{};
};

static_assert(sizeof(MStudioModel) == 112, "MStudioModel size mismatch");

struct MStudioMesh
{
    int32_t numtris{};
    int32_t triindex{};
    int32_t skinref{};
    int32_t numnorms{};
    int32_t normindex{};
};

static_assert(sizeof(MStudioMesh) == 20, "MStudioMesh size mismatch");

struct MStudioBone
{
    char name[32]{};
    int32_t parent{};
    int32_t flags{};
    int32_t bonecontroller[6]{};
    float value[6]{};
    float scale[6]{};
};

static_assert(sizeof(MStudioBone) == 112, "MStudioBone size mismatch");

struct MStudioSeqDesc
{
    char label[32]{};
    float fps{};
    int32_t flags{};
    int32_t activity{};
    int32_t actweight{};
    int32_t numevents{};
    int32_t eventindex{};
    int32_t numframes{};
    int32_t numpivots{};
    int32_t pivotindex{};
    int32_t motiontype{};
    int32_t motionbone{};
    MdlVec3 linearmovement{};
    int32_t automoveposindex{};
    int32_t automoveangleindex{};
    float bbmin[3]{};
    float bbmax[3]{};
    int32_t numblends{};
    int32_t animindex{};
    int32_t blendtype[2]{};
    float blendstart[2]{};
    float blendend[2]{};
    int32_t blendparent{};
    int32_t seqgroup{};
    int32_t entrynode{};
    int32_t exitnode{};
    int32_t nodeflags{};
    int32_t nextseq{};
};

static_assert(sizeof(MStudioSeqDesc) == 176, "MStudioSeqDesc size mismatch");

static constexpr int32_t StudioId_IDST = 0x54534449;
static constexpr int32_t StudioId_IDSQ = 0x51534449;

static constexpr int32_t StudioVersion = 10;

static constexpr int32_t STUDIO_NF_MASKED = 0x0040;
