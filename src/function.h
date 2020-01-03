#pragma once
#include "vmcode.h"

namespace kagami {
  enum ParameterPattern {
    kParamAutoSize,
    kParamAutoFill,
    kParamNormal
  };

  class _FunctionImpl {
  public:
    virtual ~_FunctionImpl() {}
  };

  class CXXFunction : public _FunctionImpl {
  private:
    Activity activity_;
  public:
    CXXFunction(Activity activity) :
      activity_(activity) {}

    Activity GetActivity() const { return activity_; }
  };

  class VMCodeFunction : public _FunctionImpl {
  private:
    VMCode code_;
    
  public:
    VMCodeFunction(VMCode ir) : code_(ir) {}

    VMCode &GetCode() { return code_; }
  };

  class ExternalFunction :public _FunctionImpl {

  };

  enum FunctionImplType {
    kFunctionCXX, kFunctionVMCode, kFunctionExternal
  };

  class FunctionImpl {
  private:
    shared_ptr<_FunctionImpl> impl_;
    ObjectMap record_;

  private:
    ParameterPattern mode_;
    FunctionImplType type_;
    size_t limit_;
    size_t offset_;
    string id_;
    vector<string> params_;

  public:
    FunctionImpl() :
      impl_(nullptr),
      record_(),
      mode_(),
      type_(kFunctionCXX),
      limit_(0),
      offset_(0),
      id_(),
      params_() {}

    FunctionImpl(
      Activity activity,
      string params,
      string id,
      ParameterPattern argument_mode = kParamNormal
    ) :
      impl_(new CXXFunction(activity)),
      record_(),
      mode_(argument_mode),
      type_(kFunctionCXX),
      limit_(0),
      offset_(0),
      id_(id),
      params_(BuildStringVector(params)) {}

    FunctionImpl(
      size_t offset,
      VMCode ir,
      string id,
      vector<string> params,
      ParameterPattern argument_mode = kParamNormal
    ) :
      impl_(new VMCodeFunction(ir)),
      record_(),
      mode_(argument_mode),
      type_(kFunctionVMCode),
      limit_(0),
      offset_(offset),
      id_(id),
      params_(params) {}

    VMCode &GetCode() {
      return dynamic_pointer_cast<VMCodeFunction>(impl_)->GetCode();
    }

    Activity GetActivity() {
      return dynamic_pointer_cast<CXXFunction>(impl_)->GetActivity();
    }

    bool operator==(FunctionImpl &rhs) const {
      if (&rhs == this) return true;
      return impl_ == rhs.impl_;
    }

    string GetId() const {
      return id_;
    }

    ParameterPattern GetPattern() const {
      return mode_;
    }

    vector<string> &GetParameters() {
      return params_;
    }

    FunctionImplType GetType() const {
      return type_;
    }

    size_t GetParamSize() const {
      return params_.size();
    }

    bool Good() const {
      return (impl_ != nullptr);
    }

    FunctionImpl &SetClosureRecord(ObjectMap record) {
      record_ = record;
      return *this;
    }

    ObjectMap &GetClosureRecord() {
      return record_;
    }

    FunctionImpl &SetLimit(size_t size) {
      limit_ = size;
      return *this;
    }

    size_t GetLimit() const {
      return limit_;
    }

    size_t GetOffset() const {
      return offset_;
    }
  };
  
  Message MakeInvokePoint(string id, string type_id = kTypeIdNull);

  using FunctionImplPointer = FunctionImpl *;
}
