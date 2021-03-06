#include "mocca/log/LogManager.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <sstream>

//#define WRITE_SHADERS 1

#ifdef WRITE_SHADERS
# include <fstream>
# include <iterator>
#endif
#include <iostream>

#include <silverbullet/math/MathTools.h>
#include "common/MemBlockPool.h"
#include <common/TrinityError.h>
#include "VisibilityState.h"
#include <opengl-base/GLProgram.h>
#include "GLVolumePool.h"
#include <silverbullet/math/MinMaxBlock.h>

enum BrickIDFlags {
  BI_MISSING = 0,
  BI_CHILD_EMPTY,
  BI_EMPTY,
  BI_FLAG_COUNT
};

using namespace Core;
using namespace Core::Math;
using namespace Core::Time;
using namespace trinity;

const uint32_t asyncGetThreadWaitSecs = 5;

static Vec3ui GetLoDSize(const Vec3ui& volumeSize, uint32_t iLoD) {
  Vec3ui vLoDSize(uint32_t(ceil(double(volumeSize.x)/MathTools::pow2(iLoD))),
                  uint32_t(ceil(double(volumeSize.y)/MathTools::pow2(iLoD))),
                  uint32_t(ceil(double(volumeSize.z)/MathTools::pow2(iLoD))));
  return vLoDSize;
}

// TODO: replace this function with dataset API call
static FLOATVECTOR3 GetFloatBrickLayout(const Vec3ui& volumeSize,
                                        const Vec3ui& maxInnerBrickSize,
                                        uint32_t iLoD) {
  FLOATVECTOR3 baseBrickCount(float(volumeSize.x)/maxInnerBrickSize.x,
                              float(volumeSize.y)/maxInnerBrickSize.y,
                              float(volumeSize.z)/maxInnerBrickSize.z);
  
  baseBrickCount /= float( MathTools::pow2(iLoD));
  
  // subtract smallest possible floating point epsilon from integer values
  // that would mess up the brick index computation in the shader
  if (float(uint32_t(baseBrickCount.x)) == baseBrickCount.x)
    baseBrickCount.x -= baseBrickCount.x * std::numeric_limits<float>::epsilon();
  if (float(uint32_t(baseBrickCount.y)) == baseBrickCount.y)
    baseBrickCount.y -= baseBrickCount.y * std::numeric_limits<float>::epsilon();
  if (float(uint32_t(baseBrickCount.z)) == baseBrickCount.z)
    baseBrickCount.z -= baseBrickCount.z * std::numeric_limits<float>::epsilon();
  
  return baseBrickCount;
}

BrickKey IndexFrom4D(const std::vector <std::vector<Vec3ui>>& loDInfoCache,
                     uint64_t modality,
                     const Vec4ui& four,
                     size_t timestep) {
  // the fourth component represents the LOD.
  const size_t lod = static_cast<size_t>(four.w);
  
  const Vec3ui layout = loDInfoCache[modality][lod];
  
  const BrickKey k = BrickKey(modality, timestep, lod, four.x +
                              four.y*layout.x +
                              four.z*layout.x*layout.y);
  return k;
}

// TODO: replace this function with dataset API call
static Vec3ui GetBrickLayout(const Vec3ui& volumeSize,
                             const Vec3ui& maxInnerBrickSize,
                             uint32_t iLoD) {
  Vec3ui baseBrickCount(uint32_t(ceil(double(volumeSize.x)/maxInnerBrickSize.x)),
                        uint32_t(ceil(double(volumeSize.y)/maxInnerBrickSize.y)),
                        uint32_t(ceil(double(volumeSize.z)/maxInnerBrickSize.z)));
  return GetLoDSize(baseBrickCount, iLoD);
}

GLVolumePool::GLVolumePool(uint64_t GPUMemorySizeInByte,
                           IIO& pDataset,
                           uint64_t modality,
                           GLenum filter,
                           bool bUseGLCore,
                           DebugMode dm)
: m_dataset(pDataset)
, m_pPoolMetadataTexture(NULL),
m_pPoolDataTexture(NULL),
m_vPoolCapacity(0,0,0),
m_maxInnerBrickSize(Vec3ui(m_dataset.getMaxUsedBrickSizes()) -
                    Vec3ui(m_dataset.getBrickOverlapSize()) * 2),
m_maxTotalBrickSize(m_dataset.getMaxUsedBrickSizes()),
m_volumeSize(m_dataset.getDomainSize(0,modality)),
m_iLoDCount(uint32_t(m_dataset.getLargestSingleBrickLOD(0)+1)),
m_filter(filter),
m_iTimeOfCreation(2),
m_iMetaTextureUnit(0),
m_iDataTextureUnit(1),
m_bUseGLCore(bUseGLCore),
m_iInsertPos(0),
m_bVisibilityUpdated(false)
, m_currentTimestep(0)
, m_currentModality(0)
, m_eDebugMode(dm)
, m_brickGetterThread(nullptr)
{
  trinity::IIO::ValueType type = m_dataset.getType(m_currentModality);
  
  switch(type){
    case IIO::ValueType::T_FLOAT :  m_sDataSetCache.m_iBitWidth = 32;
      m_sDataSetCache.m_iIsFloat = true;
      m_sDataSetCache.m_iIsSigned = true;
      break;
    case IIO::ValueType::T_DOUBLE : m_sDataSetCache.m_iBitWidth = 64;
      m_sDataSetCache.m_iIsFloat = true;
      m_sDataSetCache.m_iIsSigned = true;
      break;
    case IIO::ValueType::T_UINT8 :  m_sDataSetCache.m_iBitWidth = 8;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = false;
      break;
    case IIO::ValueType::T_UINT16 : m_sDataSetCache.m_iBitWidth = 16;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = false;
      break;
    case IIO::ValueType::T_UINT32 : m_sDataSetCache.m_iBitWidth = 32;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = false;
      break;
    case IIO::ValueType::T_UINT64 : m_sDataSetCache.m_iBitWidth = 64;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = false;
      break;
    case IIO::ValueType::T_INT8 :   m_sDataSetCache.m_iBitWidth = 8;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = true;
      break;
    case IIO::ValueType::T_INT16 :  m_sDataSetCache.m_iBitWidth = 16;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = true;
      break;
    case IIO::ValueType::T_INT32 :  m_sDataSetCache.m_iBitWidth = 32;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = true;
      break;
    case IIO::ValueType::T_INT64 :  m_sDataSetCache.m_iBitWidth = 64;
      m_sDataSetCache.m_iIsFloat = false;
      m_sDataSetCache.m_iIsSigned = true;
      break;
  }
  
  m_sDataSetCache.m_vRange = m_dataset.getRange(modality);
  
  IIO::Semantic semantic = m_dataset.getSemantic(modality);
  switch(semantic) {
    case IIO::Semantic::Scalar  : m_sDataSetCache.m_iCompCount = 1;break;
    case IIO::Semantic::Vector  : m_sDataSetCache.m_iCompCount = 3;break;
    case IIO::Semantic::Color   : m_sDataSetCache.m_iCompCount = 4;break;
  }
  
  switch (m_sDataSetCache.m_iCompCount) {
    case 1: m_format = GL_RED; break;
    case 3 : m_format = GL_RGB; break;
    case 4 : m_format = GL_RGBA; break;
    default : throw TrinityError("Invalid Component Count", __FILE__, __LINE__);
  }
  
  switch (m_sDataSetCache.m_iBitWidth) {
    case 8 :  {
      m_type = GL_UNSIGNED_BYTE;
      switch (m_sDataSetCache.m_iCompCount) {
        case 1 : m_internalformat = GL_R8; break;
        case 3 : m_internalformat = GL_RGB8; break;
        case 4 : m_internalformat = GL_RGBA8; break;
        default : throw TrinityError("Invalid Component Count", __FILE__, __LINE__);
      }
    }
      break;
    case 16 :  {
      m_type = GL_UNSIGNED_SHORT;
      switch (m_sDataSetCache.m_iCompCount) {
        case 1: m_internalformat = GL_R16; break;
        case 3 : m_internalformat = GL_RGB16; break;
        case 4 : m_internalformat = GL_RGBA16; break;
        default : throw TrinityError("Invalid Component Count", __FILE__, __LINE__);
      }
    }
      break;
    case 32 :  {
      m_type = GL_FLOAT;
      switch (m_sDataSetCache.m_iCompCount) {
        case 1 : m_internalformat = GL_R32F; break;
        case 3 : m_internalformat = GL_RGB32F; break;
        case 4 : m_internalformat = GL_RGBA32F; break;
        default : throw TrinityError("Invalid Component Count", __FILE__, __LINE__);
      }
    }
      break;
    default : throw TrinityError("Invalid bit width", __FILE__, __LINE__);
  }
  
  // first, try to create the volume
  const Vec3ui poolSize = createGLVolume(GPUMemorySizeInByte);
  
  // fill the pool slot information
  const Vec3ui slotLayout = poolSize/m_maxTotalBrickSize;
  m_vPoolSlotData.reserve(slotLayout.volume());
  
  for (uint32_t z = 0;z<slotLayout.z;++z) {
    for (uint32_t y = 0;y<slotLayout.y;++y) {
      for (uint32_t x = 0;x<slotLayout.x;++x) {
        m_vPoolSlotData.push_back(PoolSlotData(Vec3ui(x,y,z)));
      }
    }
  }
  
  // compute the LoD offset table, i.e. a table that holds
  // for each LoD the accumulated number of all bricks in
  // the lower levels, this is used to serialize a brick index
  uint32_t iOffset = 0;
  m_vLoDOffsetTable.resize(m_iLoDCount);
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    m_vLoDOffsetTable[i] = iOffset;
    iOffset += GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, i).volume();
  }
  
  createGLResources();
  if (!isValid()) return;
  
  // duplicate LoD size for efficient access
  m_LoDInfoCache.resize(m_dataset.getModalityCount());
  for (uint32_t modality = 0; modality < m_LoDInfoCache.size(); modality++) {
    m_LoDInfoCache[modality].resize(m_dataset.getLODLevelCount(modality));
    for (uint32_t lod = 0; lod < m_LoDInfoCache[modality].size(); lod++) {
      m_LoDInfoCache[modality][lod] = m_dataset.getBrickLayout(lod, modality);
    }
  }
  
  
  // duplicate metadata from dataset for efficient access
  LINFO("receiving brick metadata");
  m_brickMetadataCache =  m_dataset.getBrickMetaData(m_currentModality,
                                                      m_currentTimestep);
  
  LINFO("metadata transfer complete");

  switch (m_eDebugMode) {
    default:
    case DM_NONE:
    {
      // we can process 7500 bricks/ms (1500 running debug build)
      uint32_t const iAsyncUpdaterThreshold = 7500 * 5;
    }
      break;
    case DM_BUSY:
      
      break;
    case DM_SYNC:
      // if we want to disable the async updater we just don't instantiate it
      // WARNING("Forcing always synchronous metadata updates, async worker is disabled.");
      break;
    case DM_NOEMPTYSPACELEAPING:
      //WARNING("Visibility computation is DISABLED, disabling empty space leaping.");
      break;
  }
  m_brickGetterThread = mocca::make_unique<LambdaThread>(std::bind(&GLVolumePool::brickGetterFunc, this, std::placeholders::_1, std::placeholders::_2));
  m_brickGetterThread->startThread();
  
}

uint32_t GLVolumePool::getIntegerBrickID(const Vec4ui& vBrickID) const {
  Vec3ui bricks = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, vBrickID.w);
  return vBrickID.x + vBrickID.y * bricks.x + vBrickID.z * bricks.x * bricks.y + m_vLoDOffsetTable[vBrickID.w];
}

Vec4ui GLVolumePool::getVectorBrickID(uint32_t iBrickID) const {
  auto up = std::upper_bound(m_vLoDOffsetTable.cbegin(), m_vLoDOffsetTable.cend(), iBrickID);
  uint32_t lod = uint32_t(up - m_vLoDOffsetTable.cbegin()) - 1;
  Vec3ui bricks = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, lod);
  iBrickID -= m_vLoDOffsetTable[lod];
  
  return Vec4ui(iBrickID % bricks.x,
                (iBrickID % (bricks.x*bricks.y)) / bricks.x,
                iBrickID / (bricks.x*bricks.y),
                lod);
}


std::string GLVolumePool::getShaderFragment(uint32_t iMetaTextureUnit,
                                            uint32_t iDataTextureUnit,
                                            MissingBrickStrategy strategy,
                                            const std::string& WsetPrefixName)
{
  // must have created GL resources before asking for shader
  if (!m_pPoolMetadataTexture || !m_pPoolDataTexture) return "";
  
  m_iMetaTextureUnit = iMetaTextureUnit;
  m_iDataTextureUnit = iDataTextureUnit;
  
  std::stringstream ss;
  
#ifdef WRITE_SHADERS
  const char* shname = "volpool.glsl";
  std::ifstream shader(shname);
  if(shader) {
    //MESSAGE("Reusing volpool.glsl shader on disk.");
    shader >> std::noskipws;
    std::string sh(
                   (std::istream_iterator<char>(shader)),
                   (std::istream_iterator<char>())
                   );
    shader.close();
    return sh;
  }
#endif
  
  if (m_bUseGLCore)
    ss << "#version 420 core\n";
  else
    ss << "#version 420 compatibility\n";
  
  FLOATVECTOR3 poolAspect(m_pPoolDataTexture->GetSize());
  poolAspect /= poolAspect.minVal();
  
  // get the maximum precision for floats (larger precisions would just append
  // zeroes)
  ss << std::setprecision(36);
  ss << "\n";
  // HACK START: for Nvidia GeForce GTX 560 Ti
  if (m_iMetaTextureUnit > 0)
    for (uint32_t i = 0; i < m_iMetaTextureUnit; ++i)
      ss << "layout(binding=" << i << ") uniform sampler1D dummy" << i << ";\n";
  // HACK END: for Nvidia GeForce GTX 560 Ti
  ss << "layout(binding = " << m_iMetaTextureUnit << ") uniform usampler3D metaData;\n"
  
  << "#define iMetaTextureSize uvec3("
  << m_pPoolMetadataTexture->GetSize().x << ", "
  << m_pPoolMetadataTexture->GetSize().y << ", "
  << m_pPoolMetadataTexture->GetSize().z << ")\n"
  << "// #define iMetaTextureWidth " << m_pPoolMetadataTexture->GetSize().x << "\n"
  << "\n"
  << "#define BI_CHILD_EMPTY " << BI_CHILD_EMPTY << "\n"
  << "#define BI_EMPTY "       << BI_EMPTY << "\n"
  << "#define BI_MISSING "     << BI_MISSING << "\n"
  << "#define BI_FLAG_COUNT "  << BI_FLAG_COUNT << "\n"
  << "\n"
  << "layout(binding = " << m_iDataTextureUnit
  <<       ") uniform sampler3D volumePool;\n"
  << "#define iPoolSize ivec3(" << m_pPoolDataTexture->GetSize().x << ", "
  << m_pPoolDataTexture->GetSize().y << ", "
  << m_pPoolDataTexture->GetSize().z <<")\n"
  << "#define volumeSize vec3(" << m_volumeSize.x << ", "
  << m_volumeSize.y << ", "
  << m_volumeSize.z <<")\n"
  << "#define poolAspect vec3(" << poolAspect.x << ", "
  << poolAspect.y << ", "
  << poolAspect.z <<")\n"
  << "#define poolCapacity ivec3(" << m_vPoolCapacity.x << ", "
  << m_vPoolCapacity.y << ", "
  << m_vPoolCapacity.z <<")\n"
  << "// the total size of a brick in the pool, including the boundary\n"
  << "#define maxTotalBrickSize ivec3(" << m_maxTotalBrickSize.x << ", "
  << m_maxTotalBrickSize.y << ", "
  << m_maxTotalBrickSize.z <<")\n"
  << "// just the addressable (inner) size of a brick\n"
  << "#define maxInnerBrickSize  ivec3(" << m_maxInnerBrickSize.x << ", "
  << m_maxInnerBrickSize.y << ", "
  << m_maxInnerBrickSize.z <<")\n"
  << "// brick overlap voxels (in pool texcoords)\n"
  << "#define overlap vec3("
  << (m_maxTotalBrickSize.x - m_maxInnerBrickSize.x) /
  (2.0f*m_pPoolDataTexture->GetSize().x) << ", "
  << (m_maxTotalBrickSize.y - m_maxInnerBrickSize.y) /
  (2.0f*m_pPoolDataTexture->GetSize().y) << ", "
  << (m_maxTotalBrickSize.z - m_maxInnerBrickSize.z) /
  (2.0f*m_pPoolDataTexture->GetSize().z) <<")\n"
  << "uniform float fLoDFactor;\n"
  << "uniform float fLevelZeroWorldSpaceError;\n"
  << "uniform vec3 volumeAspect;\n"
  << "#define iMaxLOD " << m_iLoDCount-1 << "\n";
  if (m_iLoDCount < 2) {
    // HACK: aparently GLSL (at least NVIDIA's implementation)
    //       does not like arrays with just a single element
    //       so if we only have one LOD we just append a dummy
    //       element to each array
    ss << "uniform uint vLODOffset[2] = uint[]("
    << "uint(" << m_vLoDOffsetTable[0] << "), uint(0));\n"
    << "uniform vec3 vLODLayout[2] = vec3[](\n";
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize,
                                                m_maxInnerBrickSize, 0);
    ss << "  vec3(" << vLoDSize.x << ", " << vLoDSize.y << ", "
    << vLoDSize.z << "), // Level \n"
    << "  vec3(0.0,0.0,0.0) // Dummy \n );\n"
    // HACK:
    << "uniform uvec2 iLODLayoutSize[3] = uvec2[](\n"
    << "   uvec2(0, 0),// dummy Level that we'll never access to fix ATI/AMD issue where we cannot access the first element in a uniform integer array (does always return 0)\n";
    vLoDSize = GetFloatBrickLayout(m_volumeSize, m_maxInnerBrickSize, 0);
    ss << "  uvec2(" << unsigned(ceil(vLoDSize.x)) << ", "
    << unsigned(ceil(vLoDSize.x)) * unsigned(ceil(vLoDSize.y))
    << "), // Level 0\n"
    << "  uvec2(uint(0),uint(0)) // Dummy \n );\n";
  } else {
    ss << "uniform uint vLODOffset[" << m_iLoDCount << "] = uint[](";
    for (uint32_t i = 0;i<m_iLoDCount;++i) {
      ss << "uint(" << m_vLoDOffsetTable[i] << ")";
      if (i<m_iLoDCount-1) {
        ss << ", ";
      }
    }
    ss << ");\n"
    << "uniform vec3 vLODLayout[" << m_iLoDCount << "] = vec3[](\n";
    for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
      Vec3f vLoDSize = GetFloatBrickLayout(m_volumeSize,
                                           m_maxInnerBrickSize, i);
      ss << "  vec3(" << vLoDSize.x << ", " << vLoDSize.y << ", "
      << vLoDSize.z << ")";
      if (i<m_iLoDCount-1) {
        ss << ",";
      }
      ss << "// Level " << i << "\n";
    }
    ss << ");\n"
    // HACK:
    << "uniform uvec2 iLODLayoutSize[" << m_iLoDCount+1 /* +1 is only needed for the ATI/AMD fix below! */ << "] = uvec2[](\n"
    << "   uvec2(0, 0),// dummy Level that we'll never access to fix ATI/AMD issue where we cannot access the first element in a uniform integer array (does always return 0)\n";
    for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
      Vec3f vLoDSize = GetFloatBrickLayout(m_volumeSize,
                                           m_maxInnerBrickSize, i);
      ss << "   uvec2(" << unsigned(ceil(vLoDSize.x)) << ", "
      << unsigned(ceil(vLoDSize.x)) * unsigned(ceil(vLoDSize.y)) << ")";
      if (i<m_iLoDCount-1) {
        ss << ",";
      }
      ss << "// Level " << i << "\n";
    }
    ss << ");\n";
  }
  
  ss << "\n"
  << "uint Hash(uvec4 brick);\n"
  << "\n"
  << "uint ReportMissingBrick(uvec4 brick) {\n"
  << "  return Hash(brick);\n"
  << "}\n"
  << "\n";
  
  if (!WsetPrefixName.empty()) {
    ss << "uint " << WsetPrefixName << "Hash(uvec4 brick);\n"
    << "\n"
    << "uint ReportUsedBrick(uvec4 brick) {\n"
    << "  return " << WsetPrefixName << "Hash(brick);\n"
    << "}\n"
    << "\n";
  }
  
  ss << "ivec3 GetBrickIndex(uvec4 brickCoords) {\n"
  << "  uint iLODOffset  = vLODOffset[brickCoords.w];\n"
  << "  uvec2 iAMDFixedLODLayoutSize = iLODLayoutSize[brickCoords.w+1];\n" // see ATI/AMD fix above
  << "  uint iBrickIndex = iLODOffset + brickCoords.x + "
  "brickCoords.y * iAMDFixedLODLayoutSize.x + "
  "brickCoords.z * iAMDFixedLODLayoutSize.y;\n"
  "  return ivec3(iBrickIndex % iMetaTextureSize[0],\n"
  "               (iBrickIndex / iMetaTextureSize[0]) %"
  " iMetaTextureSize[1], "
  " iBrickIndex /"
  "   (iMetaTextureSize[0]*iMetaTextureSize[1]));\n"
  << "}\n"
  << "\n"
  << "uint GetBrickInfo(uvec4 brickCoords) {\n"
  << "  return texelFetch(metaData, GetBrickIndex(brickCoords), 0).r;\n"
  << "}\n"
  << "\n"
  << "uvec4 ComputeBrickCoords(vec3 normEntryCoords, uint iLOD) {\n"
  << "  return uvec4(normEntryCoords*vLODLayout[iLOD], iLOD);\n"
  << "}\n"
  << "\n"
  << "void GetBrickCorners(uvec4 brickCoords, out vec3 corners[2]) {\n"
  << "  corners[0] = vec3(brickCoords.xyz)   / vLODLayout[brickCoords.w];\n"
  << "  corners[1] = vec3(brickCoords.xyz+1) / vLODLayout[brickCoords.w];\n"
  << "}\n"
  << "\n"
  << "vec3 BrickExit(vec3 pointInBrick, vec3 dir, in vec3 corners[2]) {\n"
  << "  vec3 div = 1.0 / dir;\n"
  << "  ivec3 side = ivec3(step(0.0,div));\n"
  << "  vec3 tIntersect;\n"
  << "  tIntersect.x = (corners[side.x].x - pointInBrick.x) * div.x;\n"
  << "  tIntersect.y = (corners[side.y].y - pointInBrick.y) * div.y;\n"
  << "  tIntersect.z = (corners[side.z].z - pointInBrick.z) * div.z;\n"
  << "  return pointInBrick +\n"
  "         min(min(tIntersect.x, tIntersect.y), tIntersect.z) * dir;\n"
  << "}\n"
  << " \n"
  << "uvec3 InfoToCoords(in uint brickInfo) {\n"
  << "  uint index = brickInfo-BI_FLAG_COUNT;\n"
  << "  uvec3 vBrickCoords;\n"
  << "  vBrickCoords.x = index % poolCapacity.x;\n"
  << "  vBrickCoords.y = (index / poolCapacity.x) % poolCapacity.y;\n"
  << "  vBrickCoords.z = index / (poolCapacity.x*poolCapacity.y);\n"
  << "  return vBrickCoords;\n"
  << "}\n"
  << " \n"
  << "void BrickPoolCoords(in uint brickInfo,  out vec3 corners[2]) {\n"
  << "  uvec3 poolVoxelPos = InfoToCoords(brickInfo) * maxTotalBrickSize;\n"
  << "  corners[0] = (vec3(poolVoxelPos)                   / vec3(iPoolSize))+ overlap;\n"
  << "  corners[1] = (vec3(poolVoxelPos+maxTotalBrickSize) / vec3(iPoolSize))- overlap;\n"
  << "}\n"
  << " \n"
  << "void NormCoordsToPoolCoords(in vec3 normEntryCoords,\n"
  "in vec3 normExitCoords,\n"
  "in vec3 corners[2],\n"
  "in uint brickInfo,\n"
  "out vec3 poolEntryCoords,\n"
  "out vec3 poolExitCoords,\n"
  "out vec3 normToPoolScale,\n"
  "out vec3 normToPoolTrans) {\n"
  << "  vec3 poolCorners[2];\n"
  << "  BrickPoolCoords(brickInfo, poolCorners);\n"
  << "  normToPoolScale = (poolCorners[1]-poolCorners[0])/(corners[1]-corners[0]);\n"
  << "  normToPoolTrans = poolCorners[0]-corners[0]*normToPoolScale;\n"
  << "  poolEntryCoords  = (normEntryCoords * normToPoolScale + normToPoolTrans);\n"
  << "  poolExitCoords   = (normExitCoords  * normToPoolScale + normToPoolTrans);\n"
  << "}\n"
  << "\n"
  << "bool GetBrick(in vec3 normEntryCoords, inout uint iLOD, in vec3 direction,\n"
  << "              out vec3 poolEntryCoords, out vec3 poolExitCoords,\n"
  << "              out vec3 normExitCoords, out bool bEmpty,\n"
  << "              out vec3 normToPoolScale, out vec3 normToPoolTrans, out uvec4 brickCoords) {\n"
  << "  normEntryCoords = clamp(normEntryCoords, 0.0, 1.0);\n"
  << "  bool bFoundRequestedResolution = true;\n"
  << "  brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);\n"
  << "  uint  brickInfo   = GetBrickInfo(brickCoords);\n"
  << "  if (brickInfo == BI_MISSING) {\n"
  << "    uint iStartLOD = iLOD;\n"
  << "    ReportMissingBrick(brickCoords);\n"
  << "    // when the requested resolution is not present look for lower res\n"
  << "    bFoundRequestedResolution = false;\n"
  << "    do {\n"
  << "      iLOD++;\n"
  << "      brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);\n"
  << "      brickInfo   = GetBrickInfo(brickCoords);\n"
  << "      ";
  switch(strategy) {
    case OnlyNeeded: /* nothing. */ break;
    case RequestAll:
      ss << "if(brickInfo == BI_MISSING) ReportMissingBrick(brickCoords);\n";
      break;
    case SkipOneLevel:
      ss << "if(brickInfo == BI_MISSING && iStartLOD+1 == iLOD) {\n      "
      << "  ReportMissingBrick(brickCoords);\n      "
      << "}\n";
      break;
    case SkipTwoLevels:
      ss << "if(brickInfo == BI_MISSING && iStartLOD+2 == iLOD) {\n      "
      << "  ReportMissingBrick(brickCoords);\n      "
      << "}\n";
      break;
  }
  ss << "    } while (brickInfo == BI_MISSING);\n"
  << "  }\n"
  << "  // next line check for BI_EMPTY or BI_CHILD_EMPTY (BI_MISSING is\n"
  "  // excluded by code above!)\n"
  << "  bEmpty = (brickInfo <= BI_EMPTY);\n"
  << "  if (bEmpty) {\n"
  << "    // when we find an empty brick check if the lower resolutions are also empty\n"
  << "    for (uint ilowResLOD = iLOD+1; ilowResLOD<iMaxLOD;++ilowResLOD) {\n"
  << "      uvec4 lowResBrickCoords = ComputeBrickCoords(normEntryCoords, ilowResLOD);\n"
  << "      uint lowResBrickInfo = GetBrickInfo(lowResBrickCoords);\n"
  << "      if (lowResBrickInfo == BI_CHILD_EMPTY) {\n"
  << "        brickCoords = lowResBrickCoords;\n"
  << "        brickInfo = lowResBrickInfo;\n"
  << "        iLOD = ilowResLOD;\n"
  << "      } else {\n"
  << "        break;\n"
  << "      }\n"
  << "    }\n"
  << "  }\n"
  << "  vec3 corners[2];\n"
  << "  GetBrickCorners(brickCoords, corners);\n"
  << "  normExitCoords = BrickExit(normEntryCoords, direction, corners);\n"
  << "  if (bEmpty) \n"
  << "    return bFoundRequestedResolution;\n"
  << "  NormCoordsToPoolCoords(normEntryCoords, normExitCoords, corners,\n"
  << "                         brickInfo, poolEntryCoords, poolExitCoords,\n"
  << "                         normToPoolScale, normToPoolTrans);\n";
  
  if (!WsetPrefixName.empty()) {
    ss << "  if (bFoundRequestedResolution) \n"
    << "    ReportUsedBrick(brickCoords);\n";
  }
  
  ss << "  return bFoundRequestedResolution;\n"
  << "}\n"
  << "\n"
  << "vec3 GetSampleDelta() {\n"
  << "  return 1.0/vec3(iPoolSize);\n"
  << "}\n"
  << "\n"
  << "vec3 TransformToPoolSpace(in vec3 direction, "
  "in float sampleRateModifier) {\n"
  << "  // normalize the direction\n"
  << "  direction *= volumeSize;\n"
  << "  direction = normalize(direction);\n"
  << "  // scale to volume pool's norm coordinates\n"
  << "  direction /= vec3(iPoolSize);\n"
  << "  // do (roughly) two samples per voxel and apply user defined sample density\n"
  << "  return direction / (2.0*sampleRateModifier);\n"
  << "}\n"
  << " \n"
  << "float samplePool(vec3 coords) {\n"
  << " return texture(volumePool, coords).r;\n"
  << "}\n"
  << " \n"
  << "float samplePoolAlpha(vec3 coords) {\n"
  << " return texture(volumePool, coords).a;\n"
  << "}\n"
  << " \n"
  << "vec3 samplePool3(vec3 coords) {\n"
  << " return texture(volumePool, coords).rgb;\n"
  << "}\n"
  << " \n"
  << "vec4 samplePool4(vec3 coords) {\n"
  << " return texture(volumePool, coords);\n"
  << "}\n"
  << " \n"
  << "uint ComputeLOD(float dist) {\n"
  << "  // opengl -> negative z-axis hence the minus\n"
  << "  return min(iMaxLOD, uint(log2(fLoDFactor*(-dist)/fLevelZeroWorldSpaceError)));\n"
  << "}\n";
  
#ifdef WRITE_SHADERS
  std::ofstream vpool(shname, std::ios::trunc);
  if(vpool) {
    //MESSAGE("Writing new volpool shader.");
    const std::string& s = ss.str();
    std::copy(s.begin(), s.end(), std::ostream_iterator<char>(vpool, ""));
  }
  vpool.close();
#endif
  
  
  return ss.str();
}


void GLVolumePool::uploadBrick(uint32_t iBrickID, const Vec3ui& vVoxelSize, const void* pData,
                               size_t iInsertPos, uint64_t iTimeOfCreation)
{
  //StackTimer ubrick(PERF_POOL_UPLOAD_BRICK);
  PoolSlotData& slot = m_vPoolSlotData[iInsertPos];
  
  if (slot.containsVisibleBrick()) {
    m_brickStatus[slot.m_iBrickID] = BI_MISSING;
    
    // upload paged-out meta texel
    uploadMetadataTexel(slot.m_iBrickID);
    
    /*MESSAGE("Removing brick %i at queue position %i from pool",
     int(m_vPoolSlotData[iInsertPos].m_iBrickID),
     int(iInsertPos));*/
  }
  
  slot.m_iBrickID = iBrickID;
  slot.m_iTimeOfCreation = iTimeOfCreation;
  
  uint32_t iPoolCoordinate = slot.positionInPool().x +
  slot.positionInPool().y * m_vPoolCapacity.x +
  slot.positionInPool().z * m_vPoolCapacity.x * m_vPoolCapacity.y;
  
  //LDEBUGC("GLVolumePool","Loading brick  "<<int(m_vPoolSlotData[iInsertPos].m_iBrickID)<<" at queue position "<<int(iInsertPos)<<" (pool coord: "<<int(iPoolCoordinate)<< "="<<m_vPoolSlotData[iInsertPos].positionInPool()<<") at time "<<iTimeOfCreation);
  
  
  
  // update metadata (does NOT update the texture on the GPU)
  // this is done by the explicit UploadMetaData call to
  // only upload the updated data once all bricks have been
  // updated
  m_brickStatus[slot.m_iBrickID] = iPoolCoordinate + BI_FLAG_COUNT;
  
  // upload paged-in meta texel
  uploadMetadataTexel(slot.m_iBrickID);
  
  // upload brick to 3D texture
  m_pPoolDataTexture->SetData(slot.positionInPool() * m_maxTotalBrickSize,
                              vVoxelSize, pData);
}

void GLVolumePool::uploadFirstBrick(const BrickKey& bkey) {
  
  const Vec3ui vVoxelCount = m_dataset.getBrickVoxelCounts(bkey);
  
  //StackTimer poolGetBrick(PERF_POOL_GET_BRICK);
  
  bool success;
  auto vUploadMem = m_dataset.getBrick(bkey, success);
  
  if (!success) {
    LERROR("Error getting first brick: " << bkey);
    return;
  }
  
  uploadFirstBrick(vVoxelCount, vUploadMem->data());
}

void GLVolumePool::uploadFirstBrick(const Vec3ui& m_vVoxelSize, const void* pData) {
  uint32_t iLastBrickIndex = *(m_vLoDOffsetTable.end()-1);
  uploadBrick(iLastBrickIndex, m_vVoxelSize, pData, m_vPoolSlotData.size()-1, (std::numeric_limits<uint64_t>::max)());
}

bool GLVolumePool::uploadBrick(const BrickElemInfo& metaData, const void* pData) {
  // in this frame we already replaced all bricks (except the single low-res brick)
  // in the pool so now we should render them first
  if (m_iInsertPos >= m_vPoolSlotData.size()-1)
    return false;
  
  int32_t iBrickID = getIntegerBrickID(metaData.m_vBrickID);
  uploadBrick(iBrickID, metaData.m_vVoxelSize, pData, m_iInsertPos, m_iTimeOfCreation++);
  m_iInsertPos++;
  return true;
}

void GLVolumePool::enable(float fLoDFactor, const FLOATVECTOR3& vExtend,
                          const FLOATVECTOR3& /*vAspect */,
                          GLProgramPtr pShaderProgram) const {
  //m_pPoolMetadataTexture->Bind(m_iMetaTextureUnit);
  //m_pPoolDataTexture->Bind(m_iDataTextureUnit);
  
  pShaderProgram->Enable();
  pShaderProgram->Set("fLoDFactor", fLoDFactor);
  
  pShaderProgram->SetTexture3D("metaData", m_pPoolMetadataTexture->GetGLID(), m_iMetaTextureUnit);
  pShaderProgram->SetTexture3D("volumePool", m_pPoolDataTexture->GetGLID(), m_iDataTextureUnit);
  
  
  float fLevelZeroWorldSpaceError = (vExtend/Vec3f(m_volumeSize)).maxVal();
  pShaderProgram->Set("fLevelZeroWorldSpaceError",fLevelZeroWorldSpaceError);
}

void GLVolumePool::disable() const {
  /*m_pPoolMetadataTexture->FinishRead();*/
}

GLVolumePool::~GLVolumePool() {
  if (m_brickGetterThread) {
    m_brickGetterThread->requestThreadStop();
    // TODO: this should be larget than the network timeout
    //       replace th 30 secs by that timeout + x
    m_brickGetterThread->joinThread(30*1000);
    if (m_brickGetterThread->isRunning()) {
      LWARNING("brickGetterThread join has timed out, killing thread.");
      m_brickGetterThread->killThread();
    }
  }
  
  freeGLResources();
  LINFO("shutdown GLVolumePool");
}

static Vec3ui Fit1DIndexTo3DArray(uint64_t maxIdx, uint32_t maxArraySize) {
  // we're creating a 3D texture.. make sure it can be large enough to hold the
  // data!
  const uint64_t max_elems = uint64_t(maxArraySize) * uint64_t(maxArraySize) *
  uint64_t(maxArraySize);
  if(maxIdx > max_elems) {
    throw std::runtime_error("index texture exceeds max allowable size!");
  }
  
  Vec3ui texSize;
  
  if(maxIdx < uint64_t(maxArraySize)) {
    // fits 1D index into a single row
    texSize.x = uint32_t(maxIdx);
    texSize.y = 1;
    texSize.z = 1;
  } else if (maxIdx < uint64_t(maxArraySize)*uint64_t(maxArraySize)) {
    // fit 1D index into the smallest possible rectangle
    texSize.x = uint32_t(std::ceil(std::sqrt(double(maxIdx))));
    texSize.y = uint32_t(std::ceil(double(maxIdx)/double(texSize.x)));
    texSize.z = 1;
  } else {
    // fit 1D index into the smallest possible cuboid
    texSize.x = uint32_t(std::ceil(pow(double(maxIdx), 1.0/3.0)));
    texSize.y = uint32_t(std::ceil(double(maxIdx)/double(texSize.x * texSize.x)));
    texSize.z = uint32_t(std::ceil(double(maxIdx)/double(texSize.x * texSize.y)));
  }
  assert((uint64_t(texSize.x) * uint64_t(texSize.y) * uint64_t(texSize.z)) >= maxIdx);
  assert(texSize.x <= maxArraySize);
  assert(texSize.y <= maxArraySize);
  assert(texSize.z <= maxArraySize);
  return texSize;
}

Core::Math::Vec3ui GLVolumePool::createGLVolume(uint64_t GPUMemorySizeInByte) {
  // this code requires some explanation:
  // since it's impossible to figure out what 3D textures we can create
  // we simply try with an initial guess and if that fails sucessively
  // reduce the size until we sucessfully create a 3D texture
  Core::Math::Vec3ui poolSize;
  uint64_t reduction = 0;
  const IIO::ValueType type = m_dataset.getType(m_currentModality);
  const IIO::Semantic semantic = m_dataset.getSemantic(m_currentModality);
  const Vec3ui vMaxBS = Vec3ui(m_dataset.getMaxUsedBrickSizes());
  const uint64_t iMaxBrickCount = m_dataset.getTotalBrickCount(m_currentModality);
  do {
    poolSize = calculateVolumePoolSize(type,
                                                          semantic,
                                                          vMaxBS,
                                                          iMaxBrickCount,
                                                          GPUMemorySizeInByte,
                                                          reduction);
    
    GLvoid *pixels = 0;
    m_pPoolDataTexture = std::make_shared<GLTexture3D>(poolSize.x, poolSize.y,
                                                       poolSize.z,
                                                       m_internalformat,
                                                       m_format,
                                                       m_type,
                                                       pixels,
                                                       m_filter,
                                                       m_filter);
    if (!m_pPoolDataTexture->isValid()) {
      m_pPoolDataTexture->Delete();
      m_pPoolDataTexture = nullptr;
      
      if (reduction >= GPUMemorySizeInByte) break;
    } else {
      LDEBUGC("GLVolumePool", "Reduced memory usage by " <<
              reduction/(1024*1024) << " MB to create volume");
    }
    reduction += 1024*1024*10;  // reduce by 10 MB in each iteration
  } while (m_pPoolDataTexture==nullptr);
  
  return poolSize;
}

void GLVolumePool::createGLResources() {
  m_vPoolCapacity = Vec3ui(m_pPoolDataTexture->GetSize().x/m_maxTotalBrickSize.x,
                           m_pPoolDataTexture->GetSize().y/m_maxTotalBrickSize.y,
                           m_pPoolDataTexture->GetSize().z/m_maxTotalBrickSize.z);
  //#ifdef DEBUG_OUTS
  LDEBUGC("GLVolumePool" ,"Creating brick pool of size "<<m_pPoolDataTexture->GetSize()<<" to hold a "
          "max of "<<m_vPoolCapacity<<" bricks of size "<<m_maxTotalBrickSize<<" ("
          "addressable size "<<m_maxInnerBrickSize<<") and smaller.");
  //#endif
  
  int gpumax;
#ifndef DETECTED_OS_APPLE
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE_EXT, &gpumax);
#endif
  // last element in the offset table contains all bricks until the
  // last level + that last level itself contains one brick
  m_iTotalBrickCount = *(m_vLoDOffsetTable.end()-1)+1;
  
  Vec3ui vTexSize;
  try {
    vTexSize = Fit1DIndexTo3DArray(m_iTotalBrickCount, gpumax);
  } catch (const std::runtime_error& e) {
    // this is very unlikely but not impossible
    LERRORC("GLVolumePool",e.what());
    throw;
  }
  m_brickStatus.resize(vTexSize.volume());
  
  std::fill(m_brickStatus.begin(), m_brickStatus.end(), BI_MISSING);
#ifdef DEBUG_OUTS
  std::stringstream ss;
  ss << "[GLGridLeaper] Creating brick metadata texture of size " << vTexSize.x << " x "
  << vTexSize.y << " x " << vTexSize.z << " to effectively hold  "
  << m_iTotalBrickCount << " entries. "
  << "Consequently, " << vTexSize.volume() - m_iTotalBrickCount << " entries in "
  << "texture are wasted due to the 3D extension process.\n";
  printf(ss.str().c_str());
#endif
  m_pPoolMetadataTexture = std::make_shared<GLTexture3D>(
                                                         vTexSize.x, vTexSize.y, vTexSize.z, GL_R32UI,
                                                         GL_RED_INTEGER, GL_UNSIGNED_INT, &m_brickStatus[0]
                                                         );
  
  if (!m_pPoolMetadataTexture->isValid()) {
    m_pPoolMetadataTexture->Delete();
    m_pPoolMetadataTexture = nullptr;
    return;
  }
  
}

struct {
  bool operator() (const PoolSlotData& i, const PoolSlotData& j) const {
    return (i.m_iTimeOfCreation < j.m_iTimeOfCreation);
  }
} PoolSorter;

void GLVolumePool::uploadMetadataTexture() {
  //StackTimer poolmd(PERF_POOL_UPLOAD_METADATA);
  // DEBUG code
  
#ifdef DEBUG_OUTS
  printf("Brickpool Metadata entries:\n");
  for (size_t i = 0; i<m_iTotalBrickCount;++i) {
    switch (m_brickStatus[i]) {
      case BI_MISSING		: break;
      case BI_EMPTY:			printf("  %i is empty\n", i); break;
      case BI_CHILD_EMPTY:		printf("  %i is empty (including all children)\n", i); break;
      default:					printf("  %i loaded at position %i\n", i, int(m_brickStatus[i] - BI_FLAG_COUNT)); break;
    }
  }
#endif
  /*
   uint32_t used=0;
   for (size_t i = 0; i<m_iTotalBrickCount;++i) {
   if (m_brickStatus[i] > BI_MISSING)
   used++;
   }
   //MESSAGE("Pool Utilization %u/%u %g%%", used, m_PoolSlotData.size(), 100.0f*used/float(m_PoolSlotData.size()));
   // DEBUG code end
   */
  
  m_pPoolMetadataTexture->SetData(&m_brickStatus[0]);
}

void GLVolumePool::uploadMetadataTexel(uint32_t iBrickID) {
  //StackTimer pooltexel(PERF_POOL_UPLOAD_TEXEL);
  Vec3ui texDim = m_pPoolMetadataTexture->GetSize();
  
  Vec3ui const vSize(1, 1, 1); // size of single texel
  const uint32_t idx = iBrickID;
  const Vec3ui vOffset(idx % texDim[0], (idx/texDim[0]) % texDim[1],
                       idx / (texDim[0] * texDim[1]));
  m_pPoolMetadataTexture->SetData(vOffset, vSize, &m_brickStatus[iBrickID]);
}

void GLVolumePool::prepareForPaging() {
  //StackTimer ppage(PERF_POOL_SORT);
  std::sort(m_vPoolSlotData.begin(), m_vPoolSlotData.end(), PoolSorter);
  m_iInsertPos = 0;
}

template<IRenderer::ERenderMode eRenderMode>
bool GLVolumePool::ContainsData(const VisibilityState& visibility, uint32_t iBrickID)
{
  assert(eRenderMode == visibility.getRenderMode());
  static_assert(eRenderMode == IRenderer::ERenderMode::RM_1DTRANS ||
                eRenderMode == IRenderer::ERenderMode::RM_2DTRANS ||
                eRenderMode == IRenderer::ERenderMode::RM_ISOSURFACE ||
                eRenderMode == IRenderer::ERenderMode::RM_CLEARVIEW , "render mode not supported");
  switch (eRenderMode) {
    case IRenderer::ERenderMode::RM_1DTRANS:
      return (visibility.get1DTransfer().fMax >= m_brickMetadataCache[iBrickID].minScalar &&
              visibility.get1DTransfer().fMin <= m_brickMetadataCache[iBrickID].maxScalar);
      break;
    case IRenderer::ERenderMode::RM_2DTRANS:
      return (visibility.get2DTransfer().fMax >= m_brickMetadataCache[iBrickID].minScalar &&
              visibility.get2DTransfer().fMin <= m_brickMetadataCache[iBrickID].maxScalar) &&
      (visibility.get2DTransfer().fMaxGradient >= m_brickMetadataCache[iBrickID].minGradient &&
       visibility.get2DTransfer().fMinGradient <= m_brickMetadataCache[iBrickID].maxGradient);
      break;
    case IRenderer::ERenderMode::RM_ISOSURFACE:
      return (visibility.getIsoSurface().fIsoValue <= m_brickMetadataCache[iBrickID].maxScalar);
      break;
    case IRenderer::ERenderMode::RM_CLEARVIEW:
      return (visibility.getIsoSurfaceCV().fIsoValue1 <= m_brickMetadataCache[iBrickID].maxScalar) &&
      (visibility.getIsoSurfaceCV().fIsoValue2 <= m_brickMetadataCache[iBrickID].maxScalar);
      break;
  }
  return true;
}

template<IRenderer::ERenderMode eRenderMode>
void GLVolumePool::RecomputeVisibilityForBrickPool(const VisibilityState& visibility)
{
  assert(eRenderMode == visibility.getRenderMode());
  for (auto slot = m_vPoolSlotData.begin(); slot < m_vPoolSlotData.end(); slot++) {
    if (slot->wasEverUsed()) {
      bool const bContainsData = ContainsData<eRenderMode>(visibility, slot->m_iBrickID);
      bool const bContainedData = slot->containsVisibleBrick();
      if (bContainsData) {
        if (!bContainedData)
          slot->restore();
        uint32_t const iPoolCoordinate = slot->positionInPool().x +
        slot->positionInPool().y * m_vPoolCapacity.x +
        slot->positionInPool().z * m_vPoolCapacity.x * m_vPoolCapacity.y;
        m_brickStatus[slot->m_iBrickID] = iPoolCoordinate + BI_FLAG_COUNT;
      } else {
        if (bContainedData)
          slot->flagEmpty();
        m_brickStatus[slot->m_iBrickID] = BI_EMPTY; //! \todo remove again
      }
    }
  } // for all slots in brick pool
}


template<bool bInterruptable, IRenderer::ERenderMode eRenderMode>
Vec4ui GLVolumePool::RecomputeVisibilityForOctree(const VisibilityState& visibility
//Tuvok::ThreadClass::PredicateFunction pContinue = Tuvok::ThreadClass::PredicateFunction()
)
{
  Vec4ui vEmptyBrickCount(0, 0, 0, 0);
#ifndef _DEBUG
  uint32_t const iContinue = 375; // we approximately process 7500 bricks/ms, checking for interruption every 375 bricks allows us to pause in 0.05 ms (worst case)
#else
  uint32_t const iContinue = 75; // we'll just get 1500 bricks/ms running a debug build
#endif
  Vec3ui iChildLayout = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, 0);
  
  // evaluate child visibility for finest level
  for (uint32_t z = 0; z < iChildLayout.z; z++) {
    for (uint32_t y = 0; y < iChildLayout.y; y++) {
      for (uint32_t x = 0; x < iChildLayout.x; x++) {
        
        if (bInterruptable && !(x % iContinue)/* && pContinue*/)
          
          vEmptyBrickCount.x++; // increment total brick count
        
        Vec4ui const vBrickID(x, y, z, 0);
        uint32_t const brickIndex = getIntegerBrickID(vBrickID);
        if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
        {
          bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
          if (!bContainsData) {
            m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // finest level bricks are all child empty by definition
            vEmptyBrickCount.w++; // increment leaf empty brick count
          }
        }
      } // for x
    } // for y
  } // for z
  
  // walk up hierarchy (from finest to coarsest level) and propagate child empty visibility
  for (uint32_t iLoD = 1; iLoD < m_iLoDCount; iLoD++)
  {
    Vec3ui const iLayout = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, iLoD);
    
    // process even-sized volume
    Vec3ui const iEvenLayout = iChildLayout / 2;
    for (uint32_t z = 0; z < iEvenLayout.z; z++) {
      for (uint32_t y = 0; y < iEvenLayout.y; y++) {
        for (uint32_t x = 0; x < iEvenLayout.x; x++) {
          
          vEmptyBrickCount.x++; // increment total brick count
          
          Vec4ui const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = getIntegerBrickID(vBrickID);
          if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
            if (!bContainsData) {
              m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
              
              Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 1, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 1, 1, 0))] != BI_CHILD_EMPTY))
              {
                m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non child empty child
                vEmptyBrickCount.y++; // increment empty brick count
              } else {
                vEmptyBrickCount.z++; // increment child empty brick count
              }
            }
          }
        } // for x
      } // for y
    } // for z
    
    // process odd boundaries (if any)
    
    // plane at the end of the x-axis
    if (iChildLayout.x % 2) {
      for (uint32_t z = 0; z < iEvenLayout.z; z++) {
        for (uint32_t y = 0; y < iEvenLayout.y; y++) {
          
          vEmptyBrickCount.x++; // increment total brick count
          
          uint32_t const x = iLayout.x - 1;
          Vec4ui const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = getIntegerBrickID(vBrickID);
          if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
            if (!bContainsData) {
              m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
              
              Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 1, 0))] != BI_CHILD_EMPTY))
              {
                m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non child empty child
                vEmptyBrickCount.y++; // increment empty brick count
              } else {
                vEmptyBrickCount.z++; // increment child empty brick count
              }
            }
          }
        } // for y
      } // for z
    } // if x is odd
    
    // plane at the end of the y-axis
    if (iChildLayout.y % 2) {
      for (uint32_t z = 0; z < iEvenLayout.z; z++) {
        for (uint32_t x = 0; x < iEvenLayout.x; x++) {
          
          
          
          vEmptyBrickCount.x++; // increment total brick count
          
          uint32_t const y = iLayout.y - 1;
          Vec4ui const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = getIntegerBrickID(vBrickID);
          if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
            if (!bContainsData) {
              m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
              
              Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 1, 0))] != BI_CHILD_EMPTY))
              {
                m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
                vEmptyBrickCount.y++; // increment empty brick count
              } else {
                vEmptyBrickCount.z++; // increment child empty brick count
              }
            }
          }
        } // for x
      } // for z
    } // if y is odd
    
    // plane at the end of the z-axis
    if (iChildLayout.z % 2) {
      for (uint32_t y = 0; y < iEvenLayout.y; y++) {
        for (uint32_t x = 0; x < iEvenLayout.x; x++) {
          
          
          
          vEmptyBrickCount.x++; // increment total brick count
          
          uint32_t const z = iLayout.z - 1;
          Vec4ui const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = getIntegerBrickID(vBrickID);
          if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
            if (!bContainsData) {
              m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
              
              Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                  (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 1, 0, 0))] != BI_CHILD_EMPTY))
              {
                m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
                vEmptyBrickCount.y++; // increment empty brick count
              } else {
                vEmptyBrickCount.z++; // increment child empty brick count
              }
            }
          }
        } // for x
      } // for y
    } // if z is odd
    
    // line at the end of the x/y-axes
    if (iChildLayout.x % 2 && iChildLayout.y % 2) {
      for (uint32_t z = 0; z < iEvenLayout.z; z++) {
        
        
        vEmptyBrickCount.x++; // increment total brick count
        
        uint32_t const y = iLayout.y - 1;
        uint32_t const x = iLayout.x - 1;
        Vec4ui const vBrickID(x, y, z, iLoD);
        uint32_t const brickIndex = getIntegerBrickID(vBrickID);
        if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
        {
          bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
          if (!bContainsData) {
            m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
            
            Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
            if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 0, 1, 0))] != BI_CHILD_EMPTY))
            {
              m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              vEmptyBrickCount.y++; // increment empty brick count
            } else {
              vEmptyBrickCount.z++; // increment child empty brick count
            }
          }
        }
      } // for z
    } // if x and y are odd
    
    // line at the end of the x/z-axes
    if (iChildLayout.x % 2 && iChildLayout.z % 2) {
      for (uint32_t y = 0; y < iEvenLayout.y; y++) {
        
        
        
        vEmptyBrickCount.x++; // increment total brick count
        
        uint32_t const z = iLayout.z - 1;
        uint32_t const x = iLayout.x - 1;
        Vec4ui const vBrickID(x, y, z, iLoD);
        uint32_t const brickIndex = getIntegerBrickID(vBrickID);
        if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
        {
          bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
          if (!bContainsData) {
            m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
            
            Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
            if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(0, 1, 0, 0))] != BI_CHILD_EMPTY))
            {
              m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              vEmptyBrickCount.y++; // increment empty brick count
            } else {
              vEmptyBrickCount.z++; // increment child empty brick count
            }
          }
        }
      } // for y
    } // if x and z are odd
    
    // line at the end of the y/z-axes
    if (iChildLayout.y % 2 && iChildLayout.z % 2) {
      for (uint32_t x = 0; x < iEvenLayout.x; x++) {
        
        
        vEmptyBrickCount.x++; // increment total brick count
        
        uint32_t const z = iLayout.z - 1;
        uint32_t const y = iLayout.y - 1;
        Vec4ui const vBrickID(x, y, z, iLoD);
        uint32_t const brickIndex = getIntegerBrickID(vBrickID);
        if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
        {
          bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
          if (!bContainsData) {
            m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
            
            Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
            if ((m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                (m_brickStatus[getIntegerBrickID(childPosition + Vec4ui(1, 0, 0, 0))] != BI_CHILD_EMPTY))
            {
              m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              vEmptyBrickCount.y++; // increment empty brick count
            } else {
              vEmptyBrickCount.z++; // increment child empty brick count
            }
          }
        }
      } // for x
    } // if y and z are odd
    
    // single brick at the x/y/z corner
    if (iChildLayout.x % 2 && iChildLayout.y % 2 && iChildLayout.z % 2) {
      
      
      vEmptyBrickCount.x++; // increment total brick count
      
      uint32_t const z = iLayout.z - 1;
      uint32_t const y = iLayout.y - 1;
      uint32_t const x = iLayout.x - 1;
      Vec4ui const vBrickID(x, y, z, iLoD);
      uint32_t const brickIndex = getIntegerBrickID(vBrickID);
      if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
      {
        bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex);
        if (!bContainsData) {
          m_brickStatus[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
          
          Vec4ui const childPosition(x*2, y*2, z*2, iLoD-1);
          if (m_brickStatus[getIntegerBrickID(childPosition)] != BI_CHILD_EMPTY)
          {
            m_brickStatus[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
            vEmptyBrickCount.y++; // increment empty brick count
          } else {
            vEmptyBrickCount.z++; // increment child empty brick count
          }
        }
      }
    } // if x, y and z are odd
    
    iChildLayout = iLayout;
  } // for all levels
  
  return vEmptyBrickCount;
}


template<typename T>
void GLVolumePool::UploadBricksToBrickPoolT(const std::vector<Vec4ui>& vBrickIDs) {
  
  std::vector<BrickRequest> request;
  
  for (auto missingBrick = vBrickIDs.cbegin();
       missingBrick < vBrickIDs.cend(); missingBrick++) {
    const Vec4ui& vBrickID = *missingBrick;
    const BrickKey key = IndexFrom4D(m_LoDInfoCache, m_currentModality,
                                     vBrickID, m_currentTimestep);
    
    BrickRequest r = {vBrickID, key};
    request.push_back(r);
  }
  
  // copy upload data to getter thread
  requestBricksFromGetterThread(request);
}

void GLVolumePool::UploadBricksToBrickPool(const std::vector<Vec4ui>& vBrickIDs) {
  
  // brick debugging disabled
  if (!m_sDataSetCache.m_iIsSigned) {
    switch (m_sDataSetCache.m_iBitWidth) {
      case 8  :
        UploadBricksToBrickPoolT<uint8_t>(vBrickIDs);
        return;
      case 16 :
        UploadBricksToBrickPoolT<uint16_t>(vBrickIDs);
        return;
      case 32 :
        UploadBricksToBrickPoolT<uint32_t>(vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for an unsigned dataset",
                           __FILE__, __LINE__);
    }
  } else if (m_sDataSetCache.m_iIsFloat) {
    switch (m_sDataSetCache.m_iBitWidth) {
      case 32 :
        UploadBricksToBrickPoolT<float>(vBrickIDs);
        return;
      case 64 :
        UploadBricksToBrickPoolT<double>(vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for a float dataset",
                           __FILE__, __LINE__);
    }
  } else {
    switch (m_sDataSetCache.m_iBitWidth) {
      case 8  :
        UploadBricksToBrickPoolT<int8_t>(vBrickIDs);
        return;
      case 16 :
        UploadBricksToBrickPoolT<int16_t>(vBrickIDs);
        return;
      case 32 :
        UploadBricksToBrickPoolT<int32_t>(vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for a signed dataset",
                           __FILE__, __LINE__);
    }
  }
}

template<IRenderer::ERenderMode eRenderMode, typename T>
void GLVolumePool::PotentiallyUploadBricksToBrickPoolT(const VisibilityState& visibility,
                                                       const std::vector<Vec4ui>& vBrickIDs) {
  
  std::vector<BrickRequest> request;
  for (auto missingBrick = vBrickIDs.cbegin();
       missingBrick < vBrickIDs.cend(); missingBrick++) {
    const Vec4ui& vBrickID = *missingBrick;
    const BrickKey key = IndexFrom4D(m_LoDInfoCache, m_currentModality,
                                     vBrickID, m_currentTimestep);
    
    uint32_t const brickIndex = getIntegerBrickID(vBrickID);
    // the brick could be flagged as empty by now if the async updater
    // tested the brick after we ran the last render pass
    if (m_brickStatus[brickIndex] == BI_MISSING) {
      
      // we might not have tested the brick for visibility yet since the
      // updater's still running and we do not have a BI_UNKNOWN flag for now
      bool const bContainsData = ContainsData<eRenderMode>(visibility,
                                                           brickIndex);
      if (bContainsData) {
        BrickRequest r = {vBrickID, key};
        request.push_back(r);
      } else {
        m_brickStatus[brickIndex] = BI_EMPTY;
        uploadMetadataTexel(brickIndex);
      }
    } else if (m_brickStatus[brickIndex] < BI_FLAG_COUNT) {
      // if the updater touched the brick in the meanwhile,
      // we need to upload the meta texel
      uploadMetadataTexel(brickIndex);
    } else {
      LERROR("Error in brick metadata for brick: " << brickIndex);
    }
  }
  
  // copy upload data to getter thread
  requestBricksFromGetterThread(request);
}

template<IRenderer::ERenderMode eRenderMode>
void GLVolumePool::PotentiallyUploadBricksToBrickPool(const VisibilityState& visibility,
                                                      const std::vector<Vec4ui>& vBrickIDs) {
  unsigned int const iBitWidth = m_sDataSetCache.m_iBitWidth;
  
  // brick debugging disabled
  if (!m_sDataSetCache.m_iIsSigned) {
    switch (iBitWidth) {
      case 8  :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, uint8_t>(visibility,
                                                                  vBrickIDs);
        return;
      case 16 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, uint16_t>(visibility,
                                                                   vBrickIDs);
        return;
      case 32 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, uint32_t>(visibility,
                                                                   vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for an unsigned dataset",
                           __FILE__, __LINE__);
    }
  } else if (m_sDataSetCache.m_iIsFloat) {
    switch (iBitWidth) {
      case 32 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, float>(visibility,
                                                                vBrickIDs);
        return;
      case 64 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, double>(visibility,
                                                                 vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for a float dataset",
                           __FILE__, __LINE__);
    }
  } else {
    switch (iBitWidth) {
      case 8  :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, int8_t>(visibility,
                                                                 vBrickIDs);
        return;
      case 16 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, int16_t>(visibility,
                                                                  vBrickIDs);
        return;
      case 32 :
        PotentiallyUploadBricksToBrickPoolT<eRenderMode, int32_t>(visibility,
                                                                  vBrickIDs);
        return;
      default :
        throw TrinityError("Invalid bit width for a signed dataset",
                           __FILE__, __LINE__);
    }
  }
}

Vec4ui GLVolumePool::recomputeVisibility(const VisibilityState& visibility,
                                         uint64_t modality, size_t iTimestep,
                                         bool bForceSynchronousUpdate)
{
  // (totalProcessedBrickCount, emptyBrickCount, childEmptyBrickCount)
  Vec4ui vEmptyBrickCount(0, 0, 0, 0);
  if (m_eDebugMode == DM_NOEMPTYSPACELEAPING) {
    m_bVisibilityUpdated = true;
    return vEmptyBrickCount;
  }
  
  // fill minmax scalar acceleration data structure if timestep or modality changed
  if (m_currentTimestep != iTimestep || m_currentModality != modality) {
    m_currentTimestep = iTimestep;
    m_currentModality = modality;
    
    m_brickMetadataCache =  m_dataset.getBrickMetaData(m_currentModality,
                                                        m_currentTimestep);
    
  }
  
  // reset meta data for all bricks (BI_MISSING means that we haven't test the data for visibility until the async updater finishes)
  std::fill(m_brickStatus.begin(), m_brickStatus.end(), BI_MISSING);
  
  // TODO: if metadata texture grows too large (14 ms CPU update time for approx 2000x2000 texture) consider to
  //       update texel regions efficiently that will be toched by RecomputeVisibilityForBrickPool()
  //       updating every single texel turned out to be not efficient in this case
  
  // recompute visibility for cached bricks immediately
  switch (visibility.getRenderMode()) {
    case IRenderer::ERenderMode::RM_1DTRANS:
      RecomputeVisibilityForBrickPool<IRenderer::ERenderMode::RM_1DTRANS>(visibility);
      break;
    case IRenderer::ERenderMode::RM_2DTRANS:
      RecomputeVisibilityForBrickPool<IRenderer::ERenderMode::RM_2DTRANS>(visibility);
      break;
    case IRenderer::ERenderMode::RM_ISOSURFACE:
      RecomputeVisibilityForBrickPool<IRenderer::ERenderMode::RM_ISOSURFACE>(visibility);
      break;
    case IRenderer::ERenderMode::RM_CLEARVIEW:
      RecomputeVisibilityForBrickPool<IRenderer::ERenderMode::RM_CLEARVIEW>(visibility);
      break;
    default:
      LERRORC("GLVolumePool","Unhandled rendering mode.");
      return vEmptyBrickCount;
  }
  
  // recompute visibility for the entire hierarchy immediately
  switch (visibility.getRenderMode()) {
    case IRenderer::ERenderMode::RM_1DTRANS:
      vEmptyBrickCount = RecomputeVisibilityForOctree<false, IRenderer::ERenderMode::RM_1DTRANS>(visibility);
      break;
    case IRenderer::ERenderMode::RM_2DTRANS:
      vEmptyBrickCount = RecomputeVisibilityForOctree<false, IRenderer::ERenderMode::RM_2DTRANS>(visibility);
      break;
    case IRenderer::ERenderMode::RM_ISOSURFACE:
      vEmptyBrickCount = RecomputeVisibilityForOctree<false, IRenderer::ERenderMode::RM_ISOSURFACE>(visibility);
      break;
    case IRenderer::ERenderMode::RM_CLEARVIEW:
      vEmptyBrickCount = RecomputeVisibilityForOctree<false, IRenderer::ERenderMode::RM_CLEARVIEW>(visibility);
      break;
    default:
      LERRORC("GLVolumePool","Unhandled rendering mode.");
      return vEmptyBrickCount;
  }
  m_bVisibilityUpdated = true; // will be true after we uploaded the metadata texture in the next line
  if (vEmptyBrickCount.x != m_iTotalBrickCount) {
    //WARNING("%u of %u bricks were processed during synchronous visibility recomputation!");
  }
  uint32_t const iLeafBrickCount = m_dataset.getBrickLayout(0, 0).volume();
  uint32_t const iInternalBrickCount = m_iTotalBrickCount - iLeafBrickCount == 0 ? 1 : m_iTotalBrickCount - iLeafBrickCount;  // avoid division by zero
  
  
  LDEBUGC("GLVolumePool","Synchronously recomputed brick visibility for "
          <<vEmptyBrickCount.x<<" bricks");
  
  LDEBUGC("GLVolumePool",vEmptyBrickCount.y<<" inner bricks are EMPTY ("
          << ((static_cast<float>(vEmptyBrickCount.y)/iInternalBrickCount)*100.0f)
          <<" % of inner bricks,"
          <<((static_cast<float>(vEmptyBrickCount.y)/m_iTotalBrickCount)*100.0f)
          <<" % of all bricks)");
  
  
  LDEBUGC("GLVolumePool",vEmptyBrickCount.z<<" inner bricks are CHILD_EMPTY ("
          <<(static_cast<float>(vEmptyBrickCount.z)/iInternalBrickCount)*100.0f
          <<" % of inner bricks, "
          <<(static_cast<float>(vEmptyBrickCount.z)/m_iTotalBrickCount)*100.0f
          <<" % of all bricks)");
  
  LDEBUGC("GLVolumePool",vEmptyBrickCount.w<<" leaf bricks are empty  ("
          <<(static_cast<float>(vEmptyBrickCount.w)/iLeafBrickCount)*100.0f
          <<" % of leaf bricks, "
          <<(static_cast<float>(vEmptyBrickCount.w)/m_iTotalBrickCount)*100.0f
          <<" % of all bricks)");
  
  
  // upload new metadata to GPU
  LDEBUGC("GLVolumePool","will upload metadatatexture");
  uploadMetadataTexture();
  
  return vEmptyBrickCount;
}

void GLVolumePool::requestBricks(const std::vector<Vec4ui>& vBrickIDs,
                                 const VisibilityState& visibility)
{
  LINFO("Requesting:" << vBrickIDs.size() << " brick(s)");
  
  if (!vBrickIDs.empty())
  {
    prepareForPaging();
    
    if (!m_bVisibilityUpdated) {
      switch (visibility.getRenderMode()) {
        case IRenderer::ERenderMode::RM_1DTRANS:
          PotentiallyUploadBricksToBrickPool<IRenderer::ERenderMode::RM_1DTRANS>(visibility,
                                                                                 vBrickIDs);
          break;
        case IRenderer::ERenderMode::RM_2DTRANS:
          PotentiallyUploadBricksToBrickPool<IRenderer::ERenderMode::RM_2DTRANS>(visibility,
                                                                                 vBrickIDs);
          break;
        case IRenderer::ERenderMode::RM_ISOSURFACE:
          PotentiallyUploadBricksToBrickPool<IRenderer::ERenderMode::RM_ISOSURFACE>(visibility,
                                                                                    vBrickIDs);
          break;
        case IRenderer::ERenderMode::RM_CLEARVIEW:
          PotentiallyUploadBricksToBrickPool<IRenderer::ERenderMode::RM_CLEARVIEW>(visibility,
                                                                                   vBrickIDs);
          break;
        default:
          LERRORC("GLVolumePool","Unhandled rendering mode.");
      }
    } else {
      // visibility is updated guaranteeing that requested bricks contain data
      UploadBricksToBrickPool(vBrickIDs);
    }
  }
  
  if (!m_bVisibilityUpdated)
  {
    // we want to upload the whole meta texture when async updater is done
    uploadMetadataTexture();
    // must be set one frame delayed otherwise we might upload empty bricks
    m_bVisibilityUpdated = true;
    
    LDEBUGC("GLVolumePool","async visibility update completed for "
            << m_iTotalBrickCount << " bricks");
  }
}

void GLVolumePool::freeGLResources() {
  if (m_pPoolMetadataTexture) {
    m_pPoolMetadataTexture->Delete();
    m_pPoolMetadataTexture = nullptr;
  }
  if (m_pPoolDataTexture) {
    m_pPoolDataTexture->Delete();
    m_pPoolMetadataTexture = nullptr;
  }
}

uint64_t GLVolumePool::getCPUSize() const {
  return m_pPoolMetadataTexture->GetCPUSize() + m_pPoolDataTexture->GetCPUSize();
}

uint64_t GLVolumePool::getGPUSize() const {
  return m_pPoolMetadataTexture->GetCPUSize() + m_pPoolDataTexture->GetCPUSize();
}

void GLVolumePool::setFilterMode(GLenum filter) {
  m_filter = filter;
  m_pPoolDataTexture->SetFilter(filter, filter);
}


bool GLVolumePool::isValid() const {
  return m_pPoolMetadataTexture != nullptr &&
  m_pPoolMetadataTexture->isValid() &&
  m_pPoolDataTexture != nullptr &&
  m_pPoolDataTexture->isValid();
}

void GLVolumePool::requestBricksFromGetterThread(const std::vector<BrickRequest>& request) {

  size_t actualRequests = 0;
  // METHOD 1: add requests to queue

  for (size_t j = 0;j<request.size();++j) {
    
    if (m_brickDataCS.lock(asyncGetThreadWaitSecs)) {
      bool found = false;
      for (size_t i = 0;i<m_requestTodo.size();++i) {
        if (m_requestTodo[i] == request[j]) {
          found = true;
          break;
        }
      }
      
      if (found) {
        m_brickDataCS.unlock();
        continue;
      }

      for (size_t i = 0;i<m_requestDone.size();++i) {
        if (m_requestDone[i] == request[j]) {
          found = true;
          break;
        }
      }
      
      if (found) {
        m_brickDataCS.unlock();
        continue;
      }
      
      m_requestTodo.push_back(request[j]);
      m_brickDataCS.unlock();
      actualRequests++;

      m_brickGetterThread->resume();
    } else {
      continue;
    }
    
  }


  /*
  // METHOD 2: replace requests in queue
  {
    SCOPEDLOCK(m_brickDataCS);
    std::vector<BrickRequest> requestTodo;
    for (size_t j = 0;j<request.size();++j) {
      bool found = false;
      for (size_t i = 0;i<m_requestTodo.size();++i) {
        if (m_requestTodo[i] == request[j]) {
          found = true;
          break;
        }
      }
      if (!found)
        requestTodo.push_back(request[j]);
    }
    m_requestTodo = requestTodo;
    actualRequests = m_requestTodo.size();
  }
  m_brickGetterThread->resume();
*/

  LINFO(actualRequests << " new requests, " <<
        request.size()-actualRequests <<
        " already known");
}

uint32_t GLVolumePool::uploadBricks() {
  uint32_t iPagedBricks = 0;


  // Method 1: get the bricks request by request
  do {
    std::shared_ptr<std::vector<uint8_t>> data = nullptr;
    BrickRequest b;
    
    if (m_brickDataCS.lock(asyncGetThreadWaitSecs)) {
      if (m_requestDone.empty()) {
        m_brickDataCS.unlock();

        LINFO(iPagedBricks << " bricks uploaded");
        return iPagedBricks;
      }
      
      data = m_requestStorage[0];
      b = m_requestDone[0];
      m_requestDone.erase(m_requestDone.begin());
      m_requestStorage.erase(m_requestStorage.begin());
      m_brickDataCS.unlock();
      
    } else {
      LINFO(iPagedBricks << " bricks uploaded");
      return iPagedBricks;
    }
    
    const Vec3ui vVoxelSize = getBrickVoxelCounts(b.ID);
    if (uploadBrick(BrickElemInfo(b.ID, vVoxelSize),
                    data->data())) {
      iPagedBricks++;
    }
    
  } while (true);  // termination is handled by the return above
  
/*
  // Method 2: get all available bricks in one go
 
  SCOPEDLOCK(m_brickDataCS);
  uint32_t iPagedBricks = 0;
  
  for (size_t j = 0;j<m_requestDone.size();++j) {

    const Vec3ui vVoxelSize = getBrickVoxelCounts(m_requestDone[j].ID);
    if (uploadBrick(BrickElemInfo(m_requestDone[j].ID, vVoxelSize),
                    m_requestStorage[j]->data())) {
      iPagedBricks++;
    }
  }
  
  m_requestDone.clear();
  m_requestStorage.clear();
*/
  
  return iPagedBricks;
}


void GLVolumePool::brickGetterFunc(Predicate pContinue,
                                   LambdaThread::Interface& threadInterface) {
  LINFO("brickGetterThread starting");
  
  size_t todoCount = 0;
  if (m_brickDataCS.lock(asyncGetThreadWaitSecs)) {
    todoCount = m_requestTodo.size();
    m_brickDataCS.unlock();
  } else {
    todoCount = 0;
  }
  
  while (pContinue()) {
    if (todoCount == 0)
      threadInterface.suspend(pContinue);

    BrickRequest b;
    if (m_brickDataCS.lock(asyncGetThreadWaitSecs)) {
      todoCount = m_requestTodo.size();
      if (todoCount == 0) {
        m_brickDataCS.unlock();
        continue;
      }
      
      // get first element from todo list
      b = m_requestTodo[0];
      m_brickDataCS.unlock();
    } else {
      continue;
    }
    
    // now request the brick (outside the lock)
    bool success;
    auto vUploadMem = m_dataset.getBrick(b.key, success);
    if (!success) {
      LERROR("Error getting brick" << b.key);
      continue;
    }
    
    if (m_brickDataCS.lock(asyncGetThreadWaitSecs)) {
      // check if request still exists
      bool found = false;
      for (size_t i = 0;i<m_requestTodo.size();++i) {
        if (m_requestTodo[i] == b) {
          found = true;
          m_requestTodo.erase(m_requestTodo.begin()+i);
        }
      }
      if (!found) {
        LINFO("wasted a brick request");
        m_brickDataCS.unlock();
        continue;
      }
      
      m_requestStorage.push_back(vUploadMem);
      m_requestDone.push_back(b);
      
      todoCount = m_requestTodo.size();
      m_brickDataCS.unlock();
    } else {
      continue;
    }
    
  }
  LINFO("brickGetterThread terminating");
}

Vec3ui GLVolumePool::calculateVolumePoolSize(const IIO::ValueType type,
                                             const IIO::Semantic semantic,
                                             const Vec3ui vMaxBS,
                                             const uint64_t iMaxBrickCount,
                                             const uint64_t GPUMemorySizeInByte,
                                             const uint64_t reduction){
  
  
  uint64_t elementSize = 0;
  
  // TODO: replace this switch-madness with more reasonable code
  switch(semantic){
    case IIO::Semantic::Scalar : elementSize = 1; break;
    case IIO::Semantic::Vector : elementSize = 3; break;
    case IIO::Semantic::Color  : elementSize = 4; break;
  }
  switch(type){
    case  IIO::ValueType::T_INT8 :
    case  IIO::ValueType::T_UINT8 : break;
    case  IIO::ValueType::T_INT16 :
    case  IIO::ValueType::T_UINT16 : elementSize *= 2; break;
    case  IIO::ValueType::T_INT32 :
    case  IIO::ValueType::T_UINT32 :
    case  IIO::ValueType::T_FLOAT : elementSize *= 4; break;
    case  IIO::ValueType::T_INT64 :
    case  IIO::ValueType::T_UINT64 :
    case  IIO::ValueType::T_DOUBLE : elementSize *= 8; break;
  }
  
  
  // Compute the pool size as a (almost) cubed texture that fits
  // into the user specified GPU mem, is a multiple of the bricksize
  // and is no bigger than what OpenGL tells us is possible
  
  //Fake workaround for first :x \todo fix this
  uint64_t GPUmemoryInByte = GPUMemorySizeInByte-reduction;
  
  GLint iMaxVolumeDims;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &iMaxVolumeDims);
  const uint64_t iMaxGPUMem = GPUmemoryInByte;
  
  const uint64_t iMaxVoxelCount = iMaxGPUMem / elementSize;
  const uint64_t r3Voxels = uint64_t(pow(double(iMaxVoxelCount), 1.0 / 3.0));
  Vec3ui maxBricksForGPU;
  
  // round the starting input size (r3Voxels) to the closest multiple brick size
  // to guarantee as cubic as possible volume pools and to better fill the
  // available amount of memory
  // (e.g. it creates a 1024x512x1536 pool instead of a 512x2048x512 pool for
  // a brick size of 512^3 and 3.4 GB available memory)
  uint64_t multipleOfBrickSizeThatFitsInX = uint64_t(((float)r3Voxels / vMaxBS.x) + 0.5f) *vMaxBS.x;
  if (multipleOfBrickSizeThatFitsInX > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInX = (iMaxVolumeDims / vMaxBS.x)*vMaxBS.x;
  maxBricksForGPU.x = std::max<uint32_t>(vMaxBS.x,(uint32_t)multipleOfBrickSizeThatFitsInX);
  
  uint64_t multipleOfBrickSizeThatFitsInY = ((iMaxVoxelCount / (maxBricksForGPU.x*maxBricksForGPU.x)) / vMaxBS.y)*vMaxBS.y;
  if (multipleOfBrickSizeThatFitsInY > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInY = (iMaxVolumeDims / vMaxBS.y)*vMaxBS.y;
  maxBricksForGPU.y = std::max<uint32_t>(vMaxBS.y, (uint32_t)multipleOfBrickSizeThatFitsInY);
  
  uint64_t multipleOfBrickSizeThatFitsInZ = ((iMaxVoxelCount / (maxBricksForGPU.x*maxBricksForGPU.y)) / vMaxBS.z)*vMaxBS.z;
  if (multipleOfBrickSizeThatFitsInZ > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInZ = (iMaxVolumeDims / vMaxBS.z)*vMaxBS.z;
  maxBricksForGPU.z = std::max<uint32_t>(vMaxBS.z, (uint32_t)multipleOfBrickSizeThatFitsInZ);
  
  // the max brick layout required by the dataset
  const uint64_t r3Bricks = uint64_t(pow(double(iMaxBrickCount), 1.0 / 3.0));
  Vec3ui64 maxBricksForDataset;
  
  multipleOfBrickSizeThatFitsInX = vMaxBS.x*r3Bricks;
  if (multipleOfBrickSizeThatFitsInX > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInX = (iMaxVolumeDims / vMaxBS.x)*vMaxBS.x;
  maxBricksForDataset.x = (uint32_t)multipleOfBrickSizeThatFitsInX;
  
  multipleOfBrickSizeThatFitsInY = vMaxBS.y*uint64_t(ceil(float(iMaxBrickCount) / ((maxBricksForDataset.x / vMaxBS.x) * (maxBricksForDataset.x / vMaxBS.x))));
  if (multipleOfBrickSizeThatFitsInY > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInY = (iMaxVolumeDims / vMaxBS.y)*vMaxBS.y;
  maxBricksForDataset.y = (uint32_t)multipleOfBrickSizeThatFitsInY;
  
  multipleOfBrickSizeThatFitsInZ = vMaxBS.z*uint64_t(ceil(float(iMaxBrickCount) / ((maxBricksForDataset.x / vMaxBS.x) * (maxBricksForDataset.y / vMaxBS.y))));
  if (multipleOfBrickSizeThatFitsInZ > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInZ = (iMaxVolumeDims / vMaxBS.z)*vMaxBS.z;
  maxBricksForDataset.z = (uint32_t)multipleOfBrickSizeThatFitsInZ;
  
  // now use the smaller of the two layouts, normally that
  // would be the maxBricksForGPU but for small datasets that
  // can be rendered entirely in-core we may need less space
  const Vec3ui poolSize = (maxBricksForDataset.volume() < Vec3ui64(maxBricksForGPU).volume())
  ? Vec3ui(maxBricksForDataset)
  : maxBricksForGPU;
  
  return poolSize;
}
