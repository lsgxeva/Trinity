#include "UVFIO.h"
#include "io-base/UVFListData.h"
#include "silverbullet/io/FileTools.h"
#include "silverbullet/base/StringTools.h"

#include "common/TrinityError.h"
#include "mocca/log/LogManager.h"
#include "mocca/base/Error.h"


using namespace Core::IO::FileTools;
using namespace Core::StringTools;
using namespace Core::Math;
using namespace trinity;

#define MaxAcceptableBricksize 512

UVFIO::UVFIO(const std::string& fileId, const IListData& listData) :
  m_dataset(nullptr),
  m_filename("")
{
  LINFO("(UVFIO) initializing for file id " + fileId);
  const auto uvfListData = dynamic_cast<const UVFListData*>(&listData);

  if (uvfListData) {
    m_filename = fileId;
    if (!uvfListData->stripListerID(m_filename)) {
      throw TrinityError(std::string("invalid fileId") + fileId,
                         __FILE__, __LINE__);
    }

    
    LINFO("(UVFIO) Trying to load " + m_filename);
    
    
    if (!fileExists(m_filename)) {
      throw TrinityError(m_filename + " not found", __FILE__, __LINE__);
    }

    if (isDirectory(m_filename) || ToLowerCase(getExt(m_filename)) != "uvf" ) {
      throw TrinityError(m_filename + " is not a uvf file", __FILE__, __LINE__);
    }
    
    LINFO("(UVFIO) uvf filename passes basic checks, tring to open it");

    m_dataset = mocca::make_unique<UVFDataset>(m_filename,
                                               MaxAcceptableBricksize,
                                               false, false);
    
    LINFO("(UVFIO) successfully opened uvf file");
    
    if (!m_dataset->IsSameEndianness()) {
      throw TrinityError("dataset ist stored in incompatible endianness",
                         __FILE__, __LINE__);
    }

  } else {
    throw TrinityError("invalid listData type", __FILE__, __LINE__);
  }
}

Vec3ui64 UVFIO::getMaxBrickSize() const {
  return Vec3ui64(m_dataset->GetMaxBrickSize());
}

Vec3ui64 UVFIO::getMaxUsedBrickSizes() const {
  return Vec3ui64(m_dataset->GetMaxUsedBrickSizes());
}

MinMaxBlock UVFIO::maxMinForKey(const BrickKey& key) const {
  return m_dataset->MaxMinForKey(key);
}

uint64_t UVFIO::getLODLevelCount(uint64_t modality) const {
  return m_dataset->GetLODLevelCount();
}

uint64_t UVFIO::getNumberOfTimesteps() const {
  return m_dataset->GetNumberOfTimesteps();
}

Vec3ui64 UVFIO::getDomainSize(uint64_t lod, uint64_t modality) const {
  // HACK: assume all timesteps have same size
  return m_dataset->GetDomainSize(lod, 0);
}

Mat4d UVFIO::getTransformation(uint64_t) const {
  Vec3d scale = m_dataset->GetScale();
  Mat4d trans;
  trans.Scaling(float(scale.x), float(scale.y), float(scale.z));
  return trans;
}

Vec3ui UVFIO::getBrickOverlapSize() const {
  return m_dataset->GetBrickOverlapSize();
}

uint64_t UVFIO::getLargestSingleBrickLOD(uint64_t modality) const {
  // HACK: assume all timesteps have same size
  return m_dataset->GetLargestSingleBrickLOD(0);
}

Vec3f UVFIO::getBrickExtents(const BrickKey& key) const {
  return m_dataset->GetBrickExtents(key);
}

Vec3ui UVFIO::getBrickLayout(uint64_t lod, uint64_t modality) const {
  // HACK: assume all timesteps have same layout
  return m_dataset->GetBrickLayout(lod, 0);
}

uint64_t UVFIO::getModalityCount() const {
  return 1;
}

uint64_t UVFIO::getComponentCount(uint64_t modality) const {
  return m_dataset->GetComponentCount();
}

uint64_t UVFIO::getDefault1DTransferFunctionCount() const {
  return 1;
}

uint64_t UVFIO::getDefault2DTransferFunctionCount() const {
  //TODO
  return 1;
}

TransferFunction1D UVFIO::getDefault1DTransferFunction(uint64_t index) const {
  std::string default1DTFFilename = changeExt(m_filename, "1dt");
  
  if (fileExists(default1DTFFilename)) {
    TransferFunction1D tf(default1DTFFilename);
    
    if (tf.getSize() > 0) { // check if something usefull was in the file
      LINFO("(UVFIO) loaded user defined default 1D-tf from file " <<
            default1DTFFilename);
      return tf;
    }
  }

  LINFO("(UVFIO) using standard 1D tf ");
  
  TransferFunction1D tf(256);
  tf.setStdFunction();
  return tf;
}

/*
 TransferFunction2D UVFIO::getDefault2DTransferFunction(uint64_t index) const {
   std::string default2DTFFilename = changeExt(m_filename, "2dt");

 
 if (index != 0 )
 throw TrinityError("invalid 2D TF index", __FILE__, __LINE__);

 return
 }
 */

std::vector<uint64_t> UVFIO::get1DHistogram() const {
  //TODO
  return std::vector<uint64_t>();
}

std::vector<uint64_t> UVFIO::get2DHistogram() const {
  //TODO
  return std::vector<uint64_t>();
}

std::string UVFIO::getUserDefinedSemantic(uint64_t modality) const {
  return m_dataset->Name();
}

Vec2f UVFIO::getRange(uint64_t modality) const {
  std::pair<double,double> r = m_dataset->GetRange();
  return Vec2f(r.first, r.second);
}


uint64_t UVFIO::getTotalBrickCount(uint64_t modality) const {
  return m_dataset->GetTotalBrickCount();
}

bool UVFIO::getBrick(const BrickKey& key, std::vector<uint8_t>& data) const{
  return m_dataset->GetBrick(key, data);
}

Vec3ui UVFIO::getBrickVoxelCounts(const BrickKey& key) const {
  return m_dataset->GetBrickVoxelCounts(key);
}

IIO::ValueType UVFIO::getType(uint64_t modality) const {
  if (m_dataset->GetIsSigned()) {
    if (m_dataset->GetIsFloat()) {
      switch (m_dataset->GetBitWidth()) {
        case 32 : return ValueType::T_FLOAT;
        case 64 : return ValueType::T_DOUBLE;
        default : throw TrinityError("invalid data type", __FILE__, __LINE__);
      }
    } else { // signed integer
      switch (m_dataset->GetBitWidth()) {
        case 8 : return ValueType::T_INT8;
        case 16 : return ValueType::T_INT16;
        case 32 : return ValueType::T_INT32;
        case 64 : return ValueType::T_INT64;
        default : throw TrinityError("invalid data type", __FILE__, __LINE__);
      }
    }
  } else { // unsigned
    if (m_dataset->GetIsFloat()) {
      throw TrinityError("invalid data type", __FILE__, __LINE__);
    } else { // unsigned integer
      switch (m_dataset->GetBitWidth()) {
        case 8 : return ValueType::T_UINT8;
        case 16 : return ValueType::T_UINT16;
        case 32 : return ValueType::T_UINT32;
        case 64 : return ValueType::T_UINT64;
        default : throw TrinityError("invalid data type", __FILE__, __LINE__);
      }
    }
  }
}

IIO::Semantic UVFIO::getSemantic(uint64_t modality) const {
  if (m_dataset->GetComponentCount() == 1) {
    return Semantic::Scalar;
  } else {
    return Semantic::Color;  // UVF only support scalar or color
  }
}