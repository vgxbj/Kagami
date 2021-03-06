#include "machine.h"

namespace kagami {
  using namespace management;

  CommentedResult TypeChecking(ExpectationList &&lst,
    ObjectMap &obj_map,
    NullableList &&nullable) {
    bool result = true;
    string msg;

    for (auto &unit : lst) {
#ifdef _MSC_VER
#pragma warning(disable:4101)
#endif
      try {
        auto &obj = obj_map.at(unit.first);
        bool founded = (unit.second == obj.GetTypeId());
        bool null = find_in_list(unit.first, nullable);

        if (founded) continue;
        else if (null) continue;
        else {
          result = false;
          msg = "Expected type is " + unit.second +
            ", but object type is " + obj.GetTypeId();
          break;
        }
      }
      catch (std::out_of_range &e) {
        if (find_in_list(unit.first, nullable)) continue;
        else {
          result = false;
          msg = "Argument \"" + unit.first + "\" is missing";
          break;
        }
      }
      catch (...) {
        result = false;
        msg = "Internal error";
        break;
      }
#ifdef _MSC_VER
#pragma warning(default:4101)
#endif
    }

    return { result, msg };
  }


  PlainType FindTypeCode(string type_id) {
    PlainType type = kNotPlainType;

    if (type_id == kTypeIdInt) type = kPlainInt;
    if (type_id == kTypeIdFloat) type = kPlainFloat;
    if (type_id == kTypeIdString) type = kPlainString;
    if (type_id == kTypeIdBool) type = kPlainBool;

    return type;
  }

  bool IsIllegalStringOperator(Keyword keyword) {
    return keyword != kKeywordPlus && 
      keyword != kKeywordNotEqual && 
      keyword != kKeywordEquals;
  }

  int64_t IntProducer(Object &obj) {
    int64_t result = 0;
    switch (auto type = FindTypeCode(obj.GetTypeId()); type) {
    case kPlainInt:result = obj.Cast<int64_t>(); break;
    case kPlainFloat:result = static_cast<int64_t>(obj.Cast<double>()); break;
    case kPlainBool:result = obj.Cast<bool>() ? 1 : 0; break;
    default:break;
    }

    return result;
  }

  double FloatProducer(Object &obj) {
    double result = 0;
    switch (auto type = FindTypeCode(obj.GetTypeId()); type) {
    case kPlainFloat:result = obj.Cast<double>(); break;
    case kPlainInt:result = static_cast<double>(obj.Cast<int64_t>()); break;
    case kPlainBool:result = obj.Cast<bool>() ? 1.0 : 0.0; break;
    default:break;
    }

    return result;
  }

  string StringProducer(Object &obj) {
    string result;
    switch (auto type = FindTypeCode(obj.GetTypeId()); type) {
    case kPlainInt:result = to_string(obj.Cast<int64_t>()); break;
    case kPlainFloat:result = to_string(obj.Cast<double>()); break;
    case kPlainBool:result = obj.Cast<bool>() ? kStrTrue : kStrFalse; break;
    case kPlainString:result = obj.Cast<string>(); break;
    default:break;
    }

    return result;
  }

  bool BoolProducer(Object &obj) {
    auto type = FindTypeCode(obj.GetTypeId());
    bool result = false;

    if (type == kPlainInt) {
      int64_t value = obj.Cast<int64_t>();
      if (value > 0) result = true;
    }
    else if (type == kPlainFloat) {
      double value = obj.Cast<double>();
      if (value > 0.0) result = true;
    }
    else if (type == kPlainBool) {
      result = obj.Cast<bool>();
    }
    else if (type == kPlainString) {
      string &value = obj.Cast<string>();
      result = !value.empty();
    }

    return result;
  }
  
  string ParseRawString(const string & src) {
    string result = src;
    if (lexical::IsString(result)) result = lexical::GetRawString(result);
    return result;
  }

  void InitPlainTypesAndConstants() {
    using type::ObjectTraitsSetup;
    using namespace management;
    ObjectTraitsSetup(kTypeIdInt, PlainDeliveryImpl<int64_t>, PlainHasher<int64_t>)
      .InitComparator(PlainComparator<int64_t>);
    ObjectTraitsSetup(kTypeIdFloat, PlainDeliveryImpl<double>, PlainHasher<double>)
      .InitComparator(PlainComparator<double>);
    ObjectTraitsSetup(kTypeIdBool, PlainDeliveryImpl<bool>, PlainHasher<bool>)
      .InitComparator(PlainComparator<bool>);
    ObjectTraitsSetup(kTypeIdNull, ShallowDelivery)
      .InitConstructor(FunctionImpl([](ObjectMap &p)->Message {
        return Message().SetObject(Object());
      }, "", kTypeIdNull));

    EXPORT_CONSTANT(kTypeIdInt);
    EXPORT_CONSTANT(kTypeIdFloat);
    EXPORT_CONSTANT(kTypeIdBool);
    EXPORT_CONSTANT(kTypeIdNull);
    CreateConstantObject("kCoreFilename", Object(runtime::GetBinaryName()));
    CreateConstantObject("kCorePath", Object(runtime::GetBinaryPath()));
  }

  void ActivateComponents() {
    InitPlainTypesAndConstants();
    for (const auto func : kEmbeddedComponents) {
      func();
    }
  }

  void ReceiveExtReturningValue(void *value, void *slot, int type) {
    auto &slot_obj = *static_cast<Object *>(slot);

    if (type == kExtTypeInt) {
      auto *ret_value = static_cast<int64_t *>(value);
      slot_obj.PackContent(make_shared<int64_t>(*ret_value), kTypeIdInt);
    }
    else if (type == kExtTypeFloat) {
      auto *ret_value = static_cast<double *>(value);
      slot_obj.PackContent(make_shared<double>(*ret_value), kTypeIdFloat);
    }
    else if (type == kExtTypeBool) {
      auto *ret_value = static_cast<int *>(value);
      bool content = *ret_value == 1 ? true : false;
      slot_obj.PackContent(make_shared<bool>(content), kTypeIdBool);
    }
    else if (type == kExtTypeString) {
      const auto *ret_value = static_cast<char *>(value);
      string content(ret_value);
      slot_obj.PackContent(make_shared<string>(content), kTypeIdString);
    }
    else if (type == kExtTypeWideString) {
      const auto *ret_value = static_cast<wchar_t *>(value);
      wstring content(ret_value);
      slot_obj.PackContent(make_shared<wstring>(content), kTypeIdWideString);
    }
    else if (type == kExtTypeFunctionPointer) {
      const auto *ret_value = static_cast<GenericFunctionPointer *>(value);
      slot_obj.PackContent(make_shared<GenericFunctionPointer>(*ret_value), kTypeIdFunctionPointer);
    }
    else if (type == kExtTypeObjectPointer) {
      const auto *ret_value = static_cast<uintptr_t *>(value);
      slot_obj.PackContent(make_shared<GenericPointer>(*ret_value), kTypeIdObjectPointer);
    }
    else {
      slot_obj.PackContent(nullptr, kTypeIdNull);
    }
  }

  int PushObjectToVM(const char *id, void *ptr, const char *type_id, ExternalMemoryDisposer disposer,
    void *vm) {
    auto &machine = *static_cast<Machine *>(vm);
    Object ext_obj(ptr, disposer, string(type_id));
    auto result = machine.PushObject(string(id), ext_obj);
    return result ? 1 : 0;
  }

  void ReceiveError(void *vm, const char *msg) {
    auto &machine = *static_cast<Machine *>(vm);
    machine.PushError(string(msg));
  }

  void RuntimeFrame::Stepping() {
    if (!disable_step) idx += 1;
    disable_step = false;
  }

  void RuntimeFrame::Goto(size_t target_idx) {
    idx = target_idx - jump_offset;
    disable_step = true;
  }

  void RuntimeFrame::AddJumpRecord(size_t target_idx) {
    if (jump_stack.empty() || jump_stack.top() != target_idx) {
      jump_stack.push(target_idx);
    }
  }

  void RuntimeFrame::MakeError(string str) {
    error = true;
    msg_string = str;
  }

  void RuntimeFrame::MakeWarning(string str) {
    warning = true;
    msg_string = str;
  }

  void RuntimeFrame::RefreshReturnStack(Object obj) {
    if (!void_call) {
      return_stack.push(std::move(obj));
    }
  }

  void ConfigProcessor::ElementProcessing(ObjectTable &obj_table, string id, 
    const toml::value &elem_def, dawn::PlainWindow &window) {
    optional<SDL_Color> color_key_value = std::nullopt;

    auto type = toml::find<string>(elem_def, "type");
    auto priority_value = ExpectParameter<int64_t>(elem_def, "priority");

    if (type == "image") {
      SDL_Rect dest_rect{
        toml::find<int>(elem_def, "x"),
        toml::find<int>(elem_def, "y"),
        toml::find<int>(elem_def, "width"),
        toml::find<int>(elem_def, "height")
      };
      optional<SDL_Rect> src_rect_value = std::nullopt;
      //TODO:Texture reuse
      auto image_file = toml::find<string>(elem_def, "image_file");
      //these operations may throw std::out_or_range
      auto cropper_table = toml::expect<TOMLValueTable>(elem_def, "cropper");
      //0 - R, 1 - G, 2 - B, 3 - A
      auto color_key_array = toml::expect<toml::array>(elem_def, "color_key");

      if (cropper_table.is_ok()) {
        auto &cropper = cropper_table.unwrap();
        src_rect_value = SDL_Rect{
          int(cropper.at("x").as_integer()),
          int(cropper.at("y").as_integer()),
          int(cropper.at("width").as_integer()),
          int(cropper.at("height").as_integer())
        };
      }

      if (color_key_array.is_ok()) {
        auto array_size = color_key_array.unwrap().size();
        auto color_key = color_key_array.unwrap();

      if (array_size == 4) {
          color_key_value = SDL_Color{
            Uint8(color_key[0].as_integer()),
            Uint8(color_key[1].as_integer()),
            Uint8(color_key[2].as_integer()),
            Uint8(color_key[3].as_integer())
          };
        }
        else {
          throw _CustomError("Invalid color key");
        }
      }

      fs::path image_path(image_file);
      auto image_type_str = lexical::ToLower(image_path.extension().string());
      auto image_type = [&]() -> dawn::ImageType {
        const auto it = kImageTypeMatcher.find(image_type_str);
        if (it == kImageTypeMatcher.cend()) throw _CustomError("Unknown image type");
        return it->second;
      }();
      //Init Texture Object
      auto managed_texture = color_key_value.has_value() ?
        make_shared<dawn::Texture>(
        image_file, image_type, window.GetRenderer(),
        true, color_key_value.value()) :
        make_shared<dawn::Texture>(
        image_file, image_type, window.GetRenderer());
      Object texture_key_obj(id, kTypeIdString);
      Object texture_obj(managed_texture, kTypeIdTexture);
      obj_table.insert(make_pair(texture_key_obj, texture_obj));
      //Register element
      auto element = src_rect_value.has_value() ?
        dawn::Element(*managed_texture, src_rect_value.value(), dest_rect) :
        dawn::Element(*managed_texture, dest_rect);
      if (priority_value.has_value()) {
        element.SetPriority(int(priority_value.value()));
      }
      window.AddElement(id, element);
    }
    else if (type == "text") {
      auto x = toml::find<int>(elem_def, "x");
      auto y = toml::find<int>(elem_def, "y");
      auto text = toml::find<string>(elem_def, "text");
      auto size = toml::find<int64_t>(elem_def, "size");
      //expect?
      auto font_file = toml::find<string>(elem_def, "font");
      auto font_path = fs::path(font_file);
      string font_obj_id(kStrFontObjectHead);
      font_obj_id.append(lexical::ReplaceInvalidChar(
        font_path.filename().string()))
        .append("_")
        .append(to_string(size));

      auto wrap_length = toml::expect<Uint32>(elem_def, "wrap_length");
      auto color_key_array = toml::find<toml::array>(elem_def, "color_key");
      auto color_key = [&]() -> SDL_Color {
        SDL_Color color_key;
        auto size = color_key_array.size();
        if (size == 4) {
          color_key = SDL_Color{
            Uint8(color_key_array[0].as_integer()),
            Uint8(color_key_array[1].as_integer()),
            Uint8(color_key_array[2].as_integer()),
            Uint8(color_key_array[3].as_integer())
          };
        }
        else {
          throw _CustomError("Invalid color key");
        }
        return color_key;
      }();


      if (auto ptr = obj_stack_.Find(font_obj_id); ptr != nullptr) {
        auto &font = ptr->Cast<dawn::Font>();
        auto managed_texture = make_shared<dawn::Texture>(
          text, font, window.GetRenderer(), color_key,
          wrap_length.is_ok() ? wrap_length.unwrap() : 0);
        Object texture_key_obj(id, kTypeIdString);
        Object texture_obj(managed_texture, kTypeIdTexture);
        obj_table.insert(make_pair(texture_key_obj, texture_obj));

        SDL_Rect dest_rect{
          x, y, managed_texture->GetWidth(), managed_texture->GetHeight()
        };

        auto element = dawn::Element(*managed_texture, dest_rect);
        if (priority_value.has_value()) {
          element.SetPriority(int(priority_value.value()));
        }
        window.AddElement(id, element);
      }
      else {
        auto managed_font = make_shared<dawn::Font>(font_file, int(size));
        Object font_obj(managed_font, kTypeIdFont);
        auto &font = *managed_font;
        auto managed_texture = make_shared<dawn::Texture>(
          text, font, window.GetRenderer(), color_key,
          wrap_length.is_ok() ? wrap_length.unwrap() : 0);
        obj_stack_.CreateObject(font_obj_id, font_obj);
        Object texture_key_obj(id, kTypeIdString);
        Object texture_obj(managed_texture, kTypeIdTexture);
        obj_table.insert(make_pair(texture_key_obj, texture_obj));

        SDL_Rect dest_rect{
          x, y, managed_texture->GetWidth(), managed_texture->GetHeight()
        };

        auto element = dawn::Element(*managed_texture, dest_rect);
        if (priority_value.has_value()) {
          element.SetPriority(int(priority_value.value()));
        }
        window.AddElement(id, element);
      }
    }
    else {
      throw _CustomError("Unknown element type");
    }
  }

  void ConfigProcessor::TextureProcessing(string id, const toml::value &elem_def, 
    dawn::PlainWindow &window, ObjectTable &table) {
    auto type = toml::find<string>(elem_def, "type");

    if (type == "image") {
      optional<SDL_Color> color_key_value = std::nullopt;
      auto image_file = toml::find<string>(elem_def, "image_file");
      auto color_key_array = toml::expect<toml::array>(elem_def, "color_key");

      if (color_key_array.is_ok()) {
        auto array_size = color_key_array.unwrap().size();
        auto color_key = color_key_array.unwrap();

        if (array_size == 4) {
          color_key_value = SDL_Color{
            Uint8(color_key[0].as_integer()),
            Uint8(color_key[1].as_integer()),
            Uint8(color_key[2].as_integer()),
            Uint8(color_key[3].as_integer())
          };
        }
        else {
          throw _CustomError("Invalid color key");
        }
      }

      fs::path image_path(image_file);
      auto image_type_str = lexical::ToLower(image_path.extension().string());
      auto image_type = [&]() -> dawn::ImageType {
        const auto it = kImageTypeMatcher.find(image_type_str);
        if (it == kImageTypeMatcher.cend()) throw _CustomError("Unknown image type");
        return it->second;
      }();

      //Init Texture Object
      auto managed_texture = color_key_value.has_value() ?
        make_shared<dawn::Texture>(
        image_file, image_type, window.GetRenderer(),
        true, color_key_value.value()) :
        make_shared<dawn::Texture>(
        image_file, image_type, window.GetRenderer());
      Object texture_key_obj(id, kTypeIdString);
      Object texture_obj(managed_texture, kTypeIdTexture);

      table.insert(make_pair(texture_key_obj, texture_obj));
    }
    else if (type == "text") {
      auto text = toml::find<string>(elem_def, "text");
      auto size = toml::find<int64_t>(elem_def, "size");
      //expect?
      auto font_file = toml::find<string>(elem_def, "font");
      auto font_path = fs::path(font_file);
      string font_obj_id(kStrFontObjectHead);
      font_obj_id.append(lexical::ReplaceInvalidChar(
        font_path.filename().string()))
        .append("_")
        .append(to_string(size));

      auto wrap_length = toml::expect<Uint32>(elem_def, "wrap_length");
      auto color_key_array = toml::find<toml::array>(elem_def, "color_key");
      auto color_key = [&]() -> SDL_Color {
        SDL_Color color_key;
        auto size = color_key_array.size();
        if (size == 4) {
          color_key = SDL_Color{
            Uint8(color_key_array[0].as_integer()),
            Uint8(color_key_array[1].as_integer()),
            Uint8(color_key_array[2].as_integer()),
            Uint8(color_key_array[3].as_integer())
          };
        }
        else {
          throw _CustomError("Invalid color key");
        }
        return color_key;
      }();

      if (auto ptr = obj_stack_.Find(font_obj_id); ptr != nullptr) {
        auto &font = ptr->Cast<dawn::Font>();
        auto managed_texture = make_shared<dawn::Texture>(
          text, font, window.GetRenderer(), color_key,
          wrap_length.is_ok() ? wrap_length.unwrap() : 0);
        Object texture_key_obj(id, kTypeIdString);
        Object texture_obj(managed_texture, kTypeIdTexture);
        table.insert(make_pair(texture_key_obj, texture_obj));
      }
      else {
        auto managed_font = make_shared<dawn::Font>(font_file, int(size));
        Object font_obj(managed_font, kTypeIdFont);
        auto &font = *managed_font;
        auto managed_texture = make_shared<dawn::Texture>(
          text, font, window.GetRenderer(), color_key,
          wrap_length.is_ok() ? wrap_length.unwrap() : 0);
        obj_stack_.CreateObject(font_obj_id, font_obj);
        Object texture_key_obj(id, kTypeIdString);
        Object texture_obj(managed_texture, kTypeIdTexture);
        table.insert(make_pair(texture_key_obj, texture_obj));
      }
    }
    else {
      throw _CustomError("Unknown element type");
    }
  }

  void ConfigProcessor::RectangleProcessing(string id, const toml::value &elem_def,
    ObjectTable &table) {
    SDL_Rect rect{
      toml::find<int>(elem_def, "x"),
      toml::find<int>(elem_def, "y"),
      toml::find<int>(elem_def, "width"),
      toml::find<int>(elem_def, "height")
    };

    Object rect_key_obj(id, kTypeIdString);
    Object rect_obj(rect, kTypeIdRectangle);
    table.insert(make_pair(rect_key_obj, rect_obj));
  }

  void ConfigProcessor::InterfaceLayoutProcessing(string target_elem_id,
    const toml::value &elem_def, dawn::PlainWindow &window) {
    auto *elem = window.GetElementById(target_elem_id);

    if (elem == nullptr) {
      frame_stack_.top().MakeError("Element is not found - " + target_elem_id);
      return;
    }

    //Pypass PlainWindow class to avoid redundant querying
    auto &dest_info = elem->GetDestInfo();
    auto &src_info = elem->GetSrcInfo();

    auto select_value = [](int origin, const toml::value &def, string id) -> int {
      auto expected = toml::expect<int>(def, id);
      if (expected.is_ok()) return expected.unwrap();
      return origin;
    };

    dest_info = SDL_Rect{
      select_value(dest_info.x, elem_def, "x"),
      select_value(dest_info.y, elem_def, "y"),
      select_value(dest_info.w, elem_def, "width"),
      select_value(dest_info.h, elem_def, "height")
    };

    auto cropper = toml::expect<toml::value>(elem_def, "cropper");

    if (cropper.is_ok()) {
      src_info = SDL_Rect{
        select_value(src_info.x, cropper.unwrap(), "x"),
        select_value(src_info.y, cropper.unwrap(), "y"),
        select_value(src_info.w, cropper.unwrap(), "width"),
        select_value(src_info.h, cropper.unwrap(), "height")
      };
    }
  }

  string ConfigProcessor::GetTableVariant() {
    auto &frame = frame_stack_.top();
    string result;
    try {
      auto config = toml::find(toml_file_, "Config");

      auto file_type = toml::find(config, "filetype");

      if (file_type.as_string() != "table") {
        frame.MakeError("Expected file type is 'table'");
        return "";
      }

      auto variant_string = toml::find(config, "variant");
      result = variant_string.as_string();
    }
    catch (std::out_of_range &e) {
      frame_stack_.top().MakeError(e.what());
    }
    catch (toml::type_error &e) {
      frame_stack_.top().MakeError(e.what());
    }

    return result;
  }

  void ConfigProcessor::InitWindowFromConfig() {
    auto &frame = frame_stack_.top();
    auto &base = obj_stack_.GetBase().front();

    auto *texture_table_obj = base.Find(kStrTextureTable);
    if (texture_table_obj == nullptr) {
      base.Add(kStrTextureTable, Object(ObjectTable(), kTypeIdTable));
      texture_table_obj = base.Find(kStrTextureTable);
    }
    auto &table = texture_table_obj->Cast<ObjectTable>();

    //TODO:specific error processing
    //TODO:Default font definition
    try {
      //Init TOML Layout
      auto &layout_file = toml_file_;
      auto config = toml::find(layout_file, "Config");
      auto file_type = toml::find<string>(config, "filetype");

      if (file_type != "window") throw _CustomError("Expected file type is 'window'");

      auto &window_layout = toml::find(layout_file, "WindowLayout");
      //Create Window Object
      auto id = toml::find<string>(window_layout, "id"); //for window/texture table identifier
      auto win_width = toml::find<int64_t>(window_layout, "width");
      auto win_height = toml::find<int64_t>(window_layout, "height");
      auto rtr_value = ExpectParameter<bool>(window_layout, "real_time_refreshing");
      string title = [&]() -> string {
        auto title_value = toml::expect<string>(window_layout, "title");
        if (title_value.is_err()) {
          return "";
        }
        return title_value.unwrap();
      }();
      
      auto *obj = base.Find(id);

      auto managed_window = [&]()-> shared_ptr<void> {
        if (obj == nullptr) {
          dawn::WindowOption option;
          option.width = int(win_width);
          option.height = int(win_height);
          return make_shared<dawn::PlainWindow>(option);
        }

        if (obj->GetTypeId() != kTypeIdWindow) throw _CustomError("Invalid window object");
        return obj->Get();
      }();
      Object window_obj(managed_window, kTypeIdWindow);
      auto &window = *static_pointer_cast<dawn::PlainWindow>(managed_window);
      Object window_id_obj(int64_t(window.GetId()), kTypeIdInt);

      window.RealTimeRefreshingMode(rtr_value.has_value() ? rtr_value.value() : true);
      window.SetWindowTitle(title);

      if (obj == nullptr) {
        base.Add(id, window_obj);
      }
      else {
        window.ClearElements();
        table.erase(window_id_obj);
      }

      //Processing Elements
      auto elements = toml::find<TOMLValueTable>(layout_file, "Elements");
      auto it = table.find(window_obj);

      if (it != table.end()) {
        table.erase(it);
      }

      Object sub_table_obj(ObjectTable(), kTypeIdTable);
      table.insert(make_pair(window_id_obj, sub_table_obj));

      for (const auto &unit : elements) {
        ElementProcessing(
          sub_table_obj.Cast<ObjectTable>(),
          unit.first,
          unit.second,
          window
        );
      }
    }
    catch (std::exception &e) {
      frame.MakeError(e.what());
    }
  }

  void ConfigProcessor::InitTextureTable(ObjectTable &table, dawn::PlainWindow &window) {
    auto &frame = frame_stack_.top();

    try {
      auto &table_file = toml_file_;
      auto &config = toml::find(table_file, "Config");
      auto file_type = toml::find<string>(config, "filetype");
      auto variant_string = toml::find<string>(config, "variant");
      if (file_type != "table") throw _CustomError("Expected file type is 'table'");
      if (variant_string != "texture") throw _CustomError("Variant mismatch");

      auto table_def = toml::find<TOMLValueTable>(table_file, "Table");
      
      for (const auto &unit : table_def) {
        TextureProcessing(
          unit.first,
          unit.second,
          window,
          table
        );
      }
    }
    catch (std::exception &e) {
      frame.MakeError(e.what());
    }
  } 

  void ConfigProcessor::InitRectangleTable(ObjectTable &table) {
    auto &frame = frame_stack_.top();

    try {
      auto &table_file = toml_file_;
      auto &config = toml::find(table_file, "Config");
      auto file_type = toml::find<string>(config, "filetype");
      auto variant_string = toml::find<string>(config, "variant");
      if (file_type != "table") throw _CustomError("Expected file type is 'table'");
      if (variant_string != "rectangle") throw _CustomError("Variant mismatch");

      auto table_def = toml::find<TOMLValueTable>(table_file, "Table");

      for (const auto &unit : table_def) {
        RectangleProcessing(
          unit.first,
          unit.second,
          table
        );
      }
    }
    catch (std::exception &e) {
      frame.MakeError(e.what());
    }
  }

  void ConfigProcessor::ApplyInterfaceLayout(dawn::PlainWindow &window) {
    auto &frame = frame_stack_.top();

    try {
      auto &layout_file = toml_file_;
      auto config = toml::find(layout_file, "Config");
      auto file_type = toml::find<string>(config, "filetype");

      if (file_type != "layout") throw _CustomError("Expected type is 'interface'");

      auto interface_layout = toml::find<TOMLValueTable>(layout_file, "Layout");

      for (const auto &unit : interface_layout) {
        InterfaceLayoutProcessing(
          unit.first, 
          unit.second,
          window
        );
      }
    }
    catch (std::exception &e) {
      frame.MakeError(e.what());
    }

    if (window.GetRefreshingMode()) window.DrawElements();
  }

  void Machine::RecoverLastState() {
    frame_stack_.pop();
    code_stack_.pop_back();
    obj_stack_.Pop();
    frame_stack_.top().RefreshReturnStack();
  }

  void Machine::FinishInitalizerCalling() {
    auto instance_obj = *obj_stack_.GetCurrent().Find(kStrMe);
    instance_obj.SetDeliveringFlag();
    RecoverLastState();
    frame_stack_.top().RefreshReturnStack(instance_obj);
  }

  bool Machine::IsTailRecursion(size_t idx, VMCode *code) {
    if (code != code_stack_.back()) return false;

    auto &vmcode = *code;
    auto &current = vmcode[idx];
    bool result = false;

    if (idx == vmcode.size() - 1) {
      result = true;
    }
    else if (idx == vmcode.size() - 2) {
      bool needed_by_next_call =
        vmcode[idx + 1].first.GetKeywordValue() == kKeywordReturn &&
        vmcode[idx + 1].second.back().GetType() == kArgumentReturnStack &&
        vmcode[idx + 1].second.size() == 1;
      if (!current.first.option.void_call && needed_by_next_call) {
        result = true;
      }
    }

    return result;
  }

  bool Machine::IsTailCall(size_t idx) {
    if (frame_stack_.size() <= 1) return false;
    auto &vmcode = *code_stack_.back();
    bool result = false;

    if (idx == vmcode.size() - 1) {
      result = true;
    }
    else if (idx == vmcode.size() - 2) {
      bool needed_by_next_call = 
        vmcode[idx + 1].first.GetKeywordValue() == kKeywordReturn &&
        vmcode[idx + 1].second.back().GetType() == kArgumentReturnStack &&
        vmcode[idx + 1].second.size() == 1;
      if (!vmcode[idx].first.option.void_call && needed_by_next_call) {
        result = true;
      }
    }

    return result;
  }

  Object Machine::FetchPlainObject(Argument &arg) {
    auto type = arg.GetStringType();
    auto value = arg.GetData();
    Object obj;

    if (type == kStringTypeInt) {
      int64_t int_value;
      from_chars(value.data(), value.data() + value.size(), int_value);
      obj.PackContent(make_shared<int64_t>(int_value), kTypeIdInt);
    }
    else if (type == kStringTypeFloat) {
      double float_value;
#ifndef _MSC_VER
      //dealing with issues of charconv implementation in low-version clang
      float_value = stod(value);
#else
      from_chars(value.data(), value.data() + value.size(), float_value);
#endif
      obj.PackContent(make_shared<double>(float_value), kTypeIdFloat);
    }
    else {
      switch (type) {
      case kStringTypeBool:
        obj.PackContent(make_shared<bool>(value == kStrTrue), kTypeIdBool);
        break;
      case kStringTypeString:
        obj.PackContent(make_shared<string>(ParseRawString(value)), kTypeIdString);
        break;
      case kStringTypeIdentifier:
        obj.PackContent(make_shared<string>(value), kTypeIdString);
        break;
      default:
        break;
      }
    }

    return obj;
  }

  Object Machine::FetchFunctionObject(string id) {
    Object obj;
    auto &frame = frame_stack_.top();
    auto ptr = FindFunction(id);

    if (ptr != nullptr) {
      auto impl = *ptr;
      obj.PackContent(make_shared<FunctionImpl>(impl), kTypeIdFunction);
    }

    return obj;
  }

  Object Machine::FetchObject(Argument &arg, bool checking) {
    if (arg.GetType() == kArgumentNormal) {
      return FetchPlainObject(arg).SetDeliveringFlag();
    }

    auto &frame = frame_stack_.top();
    auto &return_stack = frame.return_stack;
    ObjectPointer ptr = nullptr;
    Object obj;

    //TODO:reconstruction
    if (arg.GetType() == kArgumentObjectStack) {
      if (!arg.option.domain.empty() || arg.option.use_last_assert) {
        if (arg.option.use_last_assert) {
          auto &base = frame.assert_rc_copy.Cast<ObjectStruct>();
          ptr = base.Find(arg.GetData());

          if (ptr != nullptr) obj.PackObject(*ptr);
          else {
            frame.MakeError("Member '" + arg.GetData() + "' is not found");
            return obj;
          }

          if (arg.option.assert_chain_tail) frame.assert_rc_copy = Object();
        }
        else if (arg.option.domain_type == kArgumentObjectStack) {
          ptr = obj_stack_.Find(arg.GetData(), arg.option.domain);

          if (ptr != nullptr) obj.PackObject(*ptr);
          else {
            frame.MakeError("Member '" + arg.GetData() + "' is not found inside " + arg.option.domain);
            return obj;
          }
        }
        else if (arg.option.domain_type == kArgumentReturnStack) {
          auto &sub_container = return_stack.top().Cast<ObjectStruct>();
          ptr = sub_container.Find(arg.GetData());
          //keep object alive
          if (ptr != nullptr) obj = *ptr;
          return_stack.pop();
        }
      }
      else {
        if (ptr = obj_stack_.Find(arg.GetData()); ptr != nullptr) {
          obj.PackObject(*ptr);
          return obj;
        }

        if (obj = GetConstantObject(arg.GetData()); obj.Null()) {
          obj = FetchFunctionObject(arg.GetData());
        }

        if (obj.Null()) {
          frame.MakeError("Object is not found - " + arg.GetData());
        }
      }
    }
    else if (arg.GetType() == kArgumentReturnStack) {
      if (!return_stack.empty()) {
        obj = return_stack.top();
        obj.SetDeliveringFlag();
        if(!checking) return_stack.pop(); 
      }
      else {
        frame.MakeError("Can't get object from stack.");
      }
    }

    return obj;
  }

  //deprecated.
  bool Machine::_FetchFunctionImpl(FunctionImplPointer &impl, string id, string type_id) {
    auto &frame = frame_stack_.top();

    //Modified version for function invoking
    if (type_id != kTypeIdNull) {
      if (impl = FindFunction(id, type_id); impl == nullptr) {
        frame.MakeError("Method is not found - " + id);
        return false;
      }

      return true;
    }
    else {
      if (impl = FindFunction(id); impl != nullptr) return true;
      ObjectPointer ptr = obj_stack_.Find(id);
      if (ptr != nullptr && ptr->GetTypeId() == kTypeIdFunction) {
        impl = &ptr->Cast<FunctionImpl>();
        return true;
      }

      frame.MakeError("Function is not found - " + id);
    }

    return false;
  }

  bool Machine::FetchFunctionImpl(FunctionImplPointer &impl, CommandPointer &command, ObjectMap &obj_map) {
    auto &frame = frame_stack_.top();
    auto id = command->first.GetInterfaceId();
    auto domain = command->first.GetInterfaceDomain();
    
    if (domain.GetType() != kArgumentNull || 
      command->first.option.use_last_assert) {
      Object obj = command->first.option.use_last_assert ?
        frame.assert_rc_copy :
        FetchObject(domain, true);

      if (frame.error) return false;

      //find method in sub-container    
      if (obj.IsSubContainer()) {
        impl = mgmt::FindFunction(id, obj.GetTypeId());

        if (impl == nullptr) {
          auto &base = obj.Cast<ObjectStruct>();
          auto *ptr = base.Find(id);
          if (ptr == nullptr) {
            frame.MakeError("Method is not found - " + id);
            return false;
          }
          if (ptr->GetTypeId() != kTypeIdFunction) {
            frame.MakeError(id + " is not a function object");
            return false;
          }

          impl = &ptr->Cast<FunctionImpl>();
          obj_map.emplace(NamedObject(kStrMe, obj));
          if (!frame.assert_rc_copy.Null()) frame.assert_rc_copy = Object();
          return true;
        }
        else {
          obj_map.emplace(NamedObject(kStrMe, obj));
          if (!frame.assert_rc_copy.Null()) frame.assert_rc_copy = Object();
          return true;
        }
      }

      if (impl = mgmt::FindFunction(id, obj.GetTypeId()); impl == nullptr) {
        frame.MakeError("Method is not found - " + id);
        return false;
      }

      obj_map.emplace(NamedObject(kStrMe, obj));
      if (!frame.assert_rc_copy.Null()) frame.assert_rc_copy = Object();
      return true;
    }
    //Plain bulit-in function and user-defined function
    //At first, Machine will querying in built-in function map,
    //and then try to fetch function object in heap.
    else {
      if (impl = FindFunction(id); impl != nullptr) {
        return true;
      }

      ObjectPointer ptr = obj_stack_.Find(id);

      if (ptr != nullptr) {
        if (ptr->GetTypeId() == kTypeIdFunction) {
          impl = &ptr->Cast<FunctionImpl>();
          return true;
        }
        else if (ptr->IsSubContainer() && ptr->GetTypeId() == kTypeIdStruct) {
          auto &base = ptr->Cast<ObjectStruct>();
          auto *initializer_obj = base.Find(kStrInitializer);
          
          if (initializer_obj == nullptr) {
            frame.MakeError("Struct doesn't have initializer");
            return false;
          }

          impl = &initializer_obj->Cast<FunctionImpl>();
          frame.initializer_calling = true;
          frame.struct_base = *ptr;
          return true;
        }
      }

      frame.MakeError("Function is not found - " + id);
    }

    return false;
  }

  void Machine::ClosureCatching(ArgumentList &args, size_t nest_end, bool closure) {
    auto &frame = frame_stack_.top();
    auto &obj_list = obj_stack_.GetBase();
    auto &origin_code = *code_stack_.back();
    size_t counter = 0, size = args.size(), nest = frame.idx;
    bool optional = false, variable = false;
    ParameterPattern argument_mode = kParamFixed;
    vector<string> params;
    VMCode code(&origin_code);

    for (size_t idx = nest + 1; idx < nest_end - frame.jump_offset; ++idx) {
      code.push_back(origin_code[idx]);
    }

    for (size_t idx = 1; idx < size; idx += 1) {
      auto id = args[idx].GetData();

      if (args[idx].option.optional_param) {
        optional = true;
        counter += 1;
      }

      if (args[idx].option.variable_param) variable = true;

      params.push_back(id);
    }

    if (optional) argument_mode = kParamAutoFill;
    if (variable) argument_mode = kParamAutoSize;

    FunctionImpl impl(nest + 1, code, args[0].GetData(), params, argument_mode);

    if (optional) {
      impl.SetLimit(params.size() - counter);
    }

    //TODO:Object Selection
    if (closure) {
      ObjectMap scope_record;
      auto &base = obj_stack_.GetBase();
      auto it = base.rbegin();
      bool flag = false;

      for (; it != base.rend(); ++it) {
        if (flag) break;

        if (it->Find(kStrUserFunc) != nullptr) flag = true;

        for (auto &unit : it->GetContent()) {
          if (unit.first == kStrThisWindow) continue;
          if (scope_record.find(unit.first) == scope_record.end()) {
            scope_record.insert(NamedObject(unit.first,
              type::CreateObjectCopy(unit.second)));
          }
        }
      }

      impl.SetClosureRecord(scope_record);
    }

    obj_stack_.CreateObject(args[0].GetData(),
      Object(make_shared<FunctionImpl>(impl), kTypeIdFunction));

    frame.Goto(nest_end + 1);
  }

  Message Machine::Invoke(Object obj, string id, const initializer_list<NamedObject> &&args) {
    FunctionImplPointer impl;
    auto &frame = frame_stack_.top();

    if (bool found = _FetchFunctionImpl(impl, id, obj.GetTypeId()); !found) {
      frame.MakeError("Method \"" + id + "\" is found in this type - " + obj.GetTypeId());
      return Message();
    }

    ObjectMap obj_map = args;
    obj_map.insert(NamedObject(kStrMe, obj));

    if (impl->GetType() == kFunctionVMCode) {
      Run(true, id, &impl->GetCode(), &obj_map, &impl->GetClosureRecord());
      Object obj = frame_stack_.top().return_stack.top();
      frame_stack_.top().return_stack.pop();
      return Message().SetObject(obj);
    }

    auto activity = impl->GetActivity();

    return activity(obj_map);
  }

  void Machine::CommandIfOrWhile(Keyword token, ArgumentList &args, size_t nest_end) {
    auto &frame = frame_stack_.top();
    auto &code = code_stack_.front();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument for condition is missing");
      return;
    }

    if (token == kKeywordIf || token == kKeywordWhile) {
      frame.AddJumpRecord(nest_end);
      code->FindJumpRecord(frame.idx + frame.jump_offset, frame.branch_jump_stack);
    }
    
    Object obj = FetchObject(args[0]);

    if (obj.GetTypeId() != kTypeIdBool) {
      frame.MakeError("Invalid state value type.");
      return;
    }

    bool state = obj.Cast<bool>();

    if (token == kKeywordIf) {
      frame.scope_stack.push(false);
      frame.condition_stack.push(state);
      if (!state) {
        if (frame.branch_jump_stack.empty()) {
          frame.Goto(frame.jump_stack.top());
        }
        else {
          frame.Goto(frame.branch_jump_stack.top());
          frame.branch_jump_stack.pop();
        }
      }
    }
    else if (token == kKeywordElif) {
      if (frame.condition_stack.empty()) {
        frame.MakeError("Unexpected Elif");
        return;
      }

      if (frame.condition_stack.top()) {
        frame.Goto(frame.jump_stack.top());
      }
      else {
        if (state) {
          frame.condition_stack.top() = true;
        }
        else {
          if (frame.branch_jump_stack.empty()) {
            frame.Goto(frame.jump_stack.top());
          }
          else {
            frame.Goto(frame.branch_jump_stack.top());
            frame.branch_jump_stack.pop();
          }
        }
      }
    }
    else if (token == kKeywordWhile) {
      if (!frame.jump_from_end) {
        frame.scope_stack.push(true);
        obj_stack_.Push();
      }
      else {
        frame.jump_from_end = false;
      }

      if (!state) {
        frame.Goto(nest_end);
        frame.final_cycle = true;
      }
    }
  }

  void Machine::CommandForEach(ArgumentList &args, size_t nest_end) {
    auto &frame = frame_stack_.top();
    ObjectMap obj_map;

    frame.AddJumpRecord(nest_end);

    if (frame.jump_from_end) {
      ForEachChecking(args, nest_end);
      frame.jump_from_end = false;
      return;
    }

    auto unit_id = FetchObject(args[0]).Cast<string>();
    auto container_obj = FetchObject(args[1]);

    if (!type::CheckBehavior(container_obj, kContainerBehavior)) {
      frame.MakeError("Invalid container object");
      return;
    }

    auto msg = Invoke(container_obj, kStrHead);
    if (frame.error) return;
    if (!msg.HasObject()) {
      frame.MakeError("Invalid returning value from iterator");
      return;
    }

    auto empty = Invoke(container_obj, "empty");
    if (frame.error) return;
    if (!empty.HasObject() || empty.GetObj().GetTypeId() != kTypeIdBool) {
      frame.MakeError("Invalid empty() implementation");
      return;
    }
    else if (empty.GetObj().Cast<bool>()) {
      frame.Goto(nest_end);
      frame.final_cycle = true;
      obj_stack_.Push(); //avoid error
      frame.scope_stack.push(false);
      return;
    }

    auto iterator_obj = msg.GetObj();
    if (!type::CheckBehavior(iterator_obj, kIteratorBehavior)) {
      frame.MakeError("Invalid iterator object");
      return;
    }

    auto unit = Invoke(iterator_obj, "obj").GetObj();
    if (frame.error) return;

    frame.scope_stack.push(true);
    obj_stack_.Push();
    obj_stack_.CreateObject(kStrIteratorObj, iterator_obj);
    obj_stack_.CreateObject(kStrContainerKeepAliveSlot, container_obj);
    obj_stack_.CreateObject(unit_id, unit);
  }

  void Machine::ForEachChecking(ArgumentList &args, size_t nest_end) {
    auto &frame = frame_stack_.top();
    auto unit_id = FetchObject(args[0]).Cast<string>();
    auto iterator = *obj_stack_.GetCurrent().Find(kStrIteratorObj);
    auto container = *obj_stack_.GetCurrent().Find(kStrContainerKeepAliveSlot);
    ObjectMap obj_map;

    auto tail = Invoke(container, kStrTail).GetObj();
    if (frame.error) return;
    if (!type::CheckBehavior(tail, kIteratorBehavior)) {
      frame.MakeError("Invalid container object");
      return;
    }

    Invoke(iterator, "step_forward");
    if (frame.error) return;

    auto result = Invoke(iterator, kStrCompare,
      { NamedObject(kStrRightHandSide,tail) }).GetObj();
    if (frame.error) return;

    if (result.GetTypeId() != kTypeIdBool) {
      frame.MakeError("Invalid iterator object");
      return;
    }

    if (result.Cast<bool>()) {
      frame.Goto(nest_end);
      frame.final_cycle = true;
    }
    else {
      auto unit = Invoke(iterator, "obj").GetObj();
      if (frame.error) return;
      obj_stack_.CreateObject(unit_id, unit);
    }
  }

  void Machine::CommandCase(ArgumentList &args, size_t nest_end) {
    auto &frame = frame_stack_.top();
    auto &code = code_stack_.back();

    if (args.empty()) {
      frame.MakeError("Empty argument list");
      return;
    }

    frame.AddJumpRecord(nest_end);

    bool has_jump_list = 
      code->FindJumpRecord(frame.idx + frame.jump_offset, frame.branch_jump_stack);

    Object obj = FetchObject(args[0]);
    string type_id = obj.GetTypeId();

    if (!lexical::IsPlainType(type_id)) {
      frame.MakeError("Non-plain object is not supported for now");
      return;
    }

    Object sample_obj = type::CreateObjectCopy(obj);

    frame.scope_stack.push(true);
    obj_stack_.Push();
    obj_stack_.CreateObject(kStrCaseObj, sample_obj);
    frame.condition_stack.push(false);

    if (has_jump_list) {
      frame.Goto(frame.branch_jump_stack.top());
      frame.branch_jump_stack.pop();
    }
    else {
      //although I think no one will write case block without condition branch...
      frame.Goto(frame.jump_stack.top());
    }
  }

  void Machine::CommandElse() {
    auto &frame = frame_stack_.top();

    if (frame.condition_stack.empty()) {
      frame.MakeError("Unexpected 'else'");
      return;
    }

    if (frame.condition_stack.top() == true) {
      frame.Goto(frame.jump_stack.top());
    }
    else {
      frame.condition_stack.top() = true;
    }
  }

  void Machine::CommandWhen(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    bool result = false;

    if (frame.condition_stack.empty()) {
      frame.MakeError("Unexpected 'when'");
      return;
    }

    if (frame.condition_stack.top()) {
      frame.Goto(frame.jump_stack.top());
      return;
    }

    if (!args.empty()) {
      ObjectPointer ptr = obj_stack_.Find(kStrCaseObj);
      string type_id = ptr->GetTypeId();
      bool found = false;

      if (ptr == nullptr) {
        frame.MakeError("Unexpected 'when'(2)");
        return;
      }

      if (!lexical::IsPlainType(type_id)) {
        frame.MakeError("Non-plain object is not supported for now");
        return;
      }

#define COMPARE_RESULT(_Type) (ptr->Cast<_Type>() == obj.Cast<_Type>())

      for (auto it = args.rbegin(); it != args.rend(); ++it) {
        Object obj = FetchObject(*it);

        if (obj.GetTypeId() != type_id) continue;

        if (type_id == kTypeIdInt) {
          found = COMPARE_RESULT(int64_t);
        }
        else if (type_id == kTypeIdFloat) {
          found = COMPARE_RESULT(double);
        }
        else if (type_id == kTypeIdString) {
          found = COMPARE_RESULT(string);
        }
        else if (type_id == kTypeIdBool) {
          found = COMPARE_RESULT(bool);
        }

        if (found) break;
      }
#undef COMPARE_RESULT

      if (found) {
        frame.condition_stack.top() = true;
      }
      else {
        if (!frame.branch_jump_stack.empty()) {
          frame.Goto(frame.branch_jump_stack.top());
          frame.branch_jump_stack.pop();
        }
        else {
          frame.Goto(frame.jump_stack.top());
        }
      }
    }
  }

  void Machine::CommandContinueOrBreak(Keyword token, size_t escape_depth) {
    auto &frame = frame_stack_.top();
    auto &scope_stack = frame.scope_stack;

    while (escape_depth != 0) {
      frame.condition_stack.pop();
      frame.jump_stack.pop();
      if (!scope_stack.empty() && scope_stack.top()) {
        obj_stack_.Pop();
      }
      scope_stack.pop();
      escape_depth -= 1;
    }

    frame.Goto(frame.jump_stack.top());

    switch (token) {
    case kKeywordContinue:
      frame.activated_continue = true; 
      break;
    case kKeywordBreak:
      frame.activated_break = true; 
      frame.final_cycle = true;
      break;
    default:break;
    }
  }

  void Machine::CommandStructBegin(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (args.size() < 1) {
      frame.MakeError("struct identifier is missing");
      return;
    }

    if (args.size() > 2) {
      frame.MakeError("Too many arguments for struct definition");
      return;
    }

    obj_stack_.Push();
    auto super_struct_obj = args.size() == 2 ?
      FetchObject(args[1]) : Object();
    auto id_obj = FetchObject(args[0]);
    frame.struct_id = id_obj.Cast<string>();
    if (!super_struct_obj.Null()) {
      frame.super_struct_id = super_struct_obj.Cast<string>();
    }
  }

  void Machine::CommandModuleBegin(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("module identifier is missing");
      return;
    }

    obj_stack_.Push();
    auto id_obj = FetchObject(args[0]);
    //Use struct_id slot
    frame.struct_id = id_obj.Cast<string>();
  }

  void Machine::CommandConditionEnd() {
    auto &frame = frame_stack_.top();
    frame.condition_stack.pop();
    frame.jump_stack.pop();
    frame.scope_stack.pop();
    while (!frame.branch_jump_stack.empty()) frame.branch_jump_stack.pop();
  }

  void Machine::CommandLoopEnd(size_t nest) {
    auto &frame = frame_stack_.top();

    if (frame.final_cycle) {
      if (frame.activated_continue) {
        frame.Goto(nest);
        frame.activated_continue = false;
        obj_stack_.GetCurrent().Clear();
        frame.jump_from_end = true;
      }
      else {
        if (frame.activated_break) frame.activated_break = false;
        while (!frame.return_stack.empty()) frame.return_stack.pop();
        frame.jump_stack.pop();
        obj_stack_.Pop();
      }
      frame.scope_stack.pop();
      frame.final_cycle = false;
    }
    else {
      frame.Goto(nest);
      while (!frame.return_stack.empty()) frame.return_stack.pop();
      obj_stack_.GetCurrent().Clear();
      frame.jump_from_end = true;
    }
  }

  void Machine::CommandForEachEnd(size_t nest) {
    auto &frame = frame_stack_.top();

    if (frame.final_cycle) {
      if (frame.activated_continue) {
        frame.Goto(nest);
        frame.activated_continue = false;
        obj_stack_.GetCurrent().ClearExcept(kForEachExceptions);
        frame.jump_from_end = true;
      }
      else {
        if (frame.activated_break) frame.activated_break = false;
        frame.jump_stack.pop();
        obj_stack_.Pop();
      }
      if(!frame.scope_stack.empty()) frame.scope_stack.pop();
      frame.final_cycle = false;
    }
    else {
      frame.Goto(nest);
      obj_stack_.GetCurrent().ClearExcept(kForEachExceptions);
      frame.jump_from_end = true;
    }
  }

  void Machine::CommandStructEnd() {
    auto &frame = frame_stack_.top();
    auto &base = obj_stack_.GetCurrent().GetContent();
    auto managed_struct = make_shared<ObjectStruct>();
    Object *super_struct = nullptr;

    //TODO:inheritance implementation
    if (!frame.super_struct_id.empty()) {
      super_struct = obj_stack_.Find(frame.super_struct_id);
      if (super_struct == nullptr) {
        frame.MakeError("Invalid super struct");
        return;
      }

      obj_stack_.CreateObject(kStrSuperStruct, *super_struct);
    }

    //TODO:copy members from super struct
    if (super_struct != nullptr) {
      auto &super_base = super_struct->Cast<ObjectStruct>();

      for (auto &unit : super_base.GetContent()) {
        if (compare(unit.first, kStrSuperStruct, kStrStructId)) continue;
        if (unit.second.GetTypeId() != kTypeIdFunction) {
          managed_struct->Add(unit.first, type::CreateObjectCopy(unit.second));
        }
        else {
          managed_struct->Add(unit.first, unit.second);
        }
      }

      auto *super_initalizer = super_base.Find(kStrInitializer);
      if (super_initalizer != nullptr) {
        managed_struct->Add(kStrSuperStructInitializer, *super_initalizer);
      }
    }

    //TODO:copy module memebers
    if (auto *module_list_obj = obj_stack_.GetCurrent().Find(kStrModuleList);
      module_list_obj != nullptr) {
      auto &module_list = module_list_obj->Cast<ObjectArray>();
      for (auto it = module_list.begin(); it != module_list.end(); ++it) {
        auto &module = it->Cast<ObjectStruct>().GetContent();
        for (auto &unit : module) {
          if (unit.first == kStrStructId) continue;

          if (unit.second.GetTypeId() != kTypeIdFunction) {
            managed_struct->Add(unit.first, type::CreateObjectCopy(unit.second));
          }
          else {
            managed_struct->Add(unit.first, unit.second);
          }
        }
      }
    }

    for (auto &unit : base) {
      managed_struct->Replace(unit.first, unit.second);
    }

    managed_struct->Add(kStrStructId, Object(frame.struct_id));

    obj_stack_.Pop();
    obj_stack_.CreateObject(frame.struct_id, Object(managed_struct, kTypeIdStruct));
    frame.struct_id.clear();
    frame.struct_id.shrink_to_fit();
  }

  void Machine::CommandModuleEnd() {
    auto &frame = frame_stack_.top();
    auto &base = obj_stack_.GetCurrent().GetContent();
    auto managed_module = make_shared<ObjectStruct>();

    for (auto &unit : base) {
      managed_module->Add(unit.first, unit.second);
    }

    managed_module->Add(kStrStructId, Object(frame.struct_id));

    obj_stack_.Pop();
    obj_stack_.CreateObject(frame.struct_id, Object(managed_module, kTypeIdStruct));
    frame.struct_id.clear();
    frame.struct_id.shrink_to_fit();
  }

  void Machine::CommandInclude(ArgumentList &args) {
    //TODO:insert module id into !module_list
    auto &frame = frame_stack_.top();
    auto &base = obj_stack_.GetCurrent();
    auto module_obj = FetchObject(args[0]);

    if (args.size() != 1) {
      frame.MakeError("Invalid including declaration");
      return;
    }

    if (auto *mod_list_obj = base.Find(kStrModuleList); mod_list_obj != nullptr) {
      auto &mod_list = mod_list_obj->Cast<ObjectArray>();
      mod_list.push_back(module_obj);
    }
    else {
      auto managed_arr = make_shared<ObjectArray>();
      base.Add(kStrModuleList, Object(managed_arr, kTypeIdArray));
      auto &mod_list = base.Find(kStrModuleList)->Cast<ObjectArray>();
      mod_list.push_back(module_obj);
    }
  }

  void Machine::CommandSuper(ArgumentList &args) {
    //TODO:call initializer of super struct
    auto &frame = frame_stack_.top();

  }

  void Machine::CommandHash(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    auto &obj = FetchObject(args[0]).Unpack();

    if (type::IsHashable(obj)) {
      int64_t hash = type::GetHash(obj);
      frame.RefreshReturnStack(Object(make_shared<int64_t>(hash), kTypeIdInt));
    }
    else {
      frame.RefreshReturnStack(Object());
    }
  }

  void Machine::CommandSwap(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    auto &right = FetchObject(args[1]).Unpack();
    auto &left = FetchObject(args[0]).Unpack();

    left.swap(right);
  }

  void Machine::CommandBind(ArgumentList &args, bool local_value, bool ext_value) {
    using namespace type;
    auto &frame = frame_stack_.top();
    //Do not change the order!
    auto rhs = FetchObject(args[1]);
    auto lhs = FetchObject(args[0]);

    if (frame.error) return;

    if (rhs.GetMode() == kObjectDelegator || lhs.GetMode() == kObjectDelegator) {
      frame.MakeError("Trying to assign a language key constant");
      return;
    }

    if (lhs.IsRef()) {
      auto &real_lhs = lhs.Unpack();
      real_lhs = CreateObjectCopy(rhs);
      return;
    }
    else {
      string id = lhs.Cast<string>();

      if (lexical::GetStringType(id) != kStringTypeIdentifier) {
        frame.MakeError("Invalid object id");
        return;
      }

      if (!local_value && frame.struct_id.empty()) {
        ObjectPointer ptr = obj_stack_.Find(id);

        if (ptr != nullptr) {
          ptr->Unpack() = CreateObjectCopy(rhs);
          return;
        }
      }

      Object obj = CreateObjectCopy(rhs);

      if (!obj_stack_.CreateObject(id, obj)) {
        frame.MakeError("Object binding is failed");
        return;
      }
    }
  }

  void Machine::CommandDelivering(ArgumentList &args, bool local_value, bool ext_value) {
    auto &frame = frame_stack_.top();
    //Do not change the order!
    auto rhs = FetchObject(args[1]);
    auto lhs = FetchObject(args[0]);

    if (lhs.IsRef()) {
      auto &real_lhs = lhs.Unpack();
      real_lhs = rhs.Unpack();
      rhs.Unpack() = Object();
    }
    else {
      string id = lhs.Cast<string>();

      if (lexical::GetStringType(id) != kStringTypeIdentifier) {
        frame.MakeError("Invalid object id");
        return;
      }

      if (!local_value && frame.struct_id.empty()) {
        ObjectPointer ptr = obj_stack_.Find(id);

        if (ptr != nullptr) {
          ptr->Unpack() = rhs.Unpack();
          rhs.Unpack() = Object();
          return;
        }
      }

      Object obj = rhs.Unpack();
      rhs.Unpack() = Object();

      if (!obj_stack_.CreateObject(id, obj)) {
        frame.MakeError("Object delivering is failed");
        return;
      }
    }
  }

  void Machine::CommandTypeId(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (args.size() > 1) {
      ManagedArray base = make_shared<ObjectArray>();

      for (auto &unit : args) {
        base->emplace_back(Object(FetchObject(unit).GetTypeId()));
      }

      Object obj(base, kTypeIdArray);
      frame.RefreshReturnStack(obj);
    }
    else if (args.size() == 1) {
      frame.RefreshReturnStack(Object(FetchObject(args[0]).GetTypeId()));
    }
    else {
      frame.RefreshReturnStack(Object(kTypeIdNull));
    }
  }

  void Machine::CommandMethods(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - dir(obj)");
      return;
    }

    Object obj = FetchObject(args[0]);
    auto methods = type::GetMethods(obj.GetTypeId());
    ManagedArray base = make_shared<ObjectArray>();

    for (auto &unit : methods) {
      base->emplace_back(Object(unit, kTypeIdString));
    }

    if (obj.IsSubContainer()) {
      auto &container = obj.Cast<ObjectStruct>().GetContent();
      for (auto &unit : container) {
        if (unit.second.GetTypeId() == kTypeIdFunction) {
          base->emplace_back(unit.first);
        }
      }
    }

    Object ret_obj(base, kTypeIdArray);
    frame.RefreshReturnStack(ret_obj);
  }

  void Machine::CommandExist(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(2)) {
      frame.MakeError("Argument is missing  - exist(obj, id)");
      return;
    }

    //Do not change the order
    auto str_obj = FetchObject(args[1]);
    auto obj = FetchObject(args[0]);

    if (str_obj.GetTypeId() != kTypeIdString) {
      frame.MakeError("Invalid method id");
      return;
    }

    string str = str_obj.Cast<string>();
    bool first_stage = type::CheckMethod(str, obj.GetTypeId());
    bool second_stage = obj.IsSubContainer() ?
      [&]() -> bool {
      auto &container = obj.Cast<ObjectStruct>().GetContent();
      for (auto &unit : container) {
        if (unit.first == str) return true;
      }
      return false;
    }() : false;


    Object ret_obj(first_stage || second_stage, kTypeIdBool);

    frame.RefreshReturnStack(ret_obj);
  }

  void Machine::CommandNullObj(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - null_obj(obj)");
      return;
    }

    Object obj = FetchObject(args[0]);
    frame.RefreshReturnStack(Object(obj.GetTypeId() == kTypeIdNull, kTypeIdBool));
  }

  void Machine::CommandDestroy(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - destroy(obj)");
      return;
    }

    Object &obj = FetchObject(args[0]).Unpack();
    obj.swap(Object());
  }

  void Machine::CommandConvert(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - convert(obj)");
      return;
    }

    Argument &arg = args[0];
    if (arg.GetType() == kArgumentNormal) {
      FetchPlainObject(arg);
    }
    else {
      Object obj = FetchObject(args[0]);
      string type_id = obj.GetTypeId();
      Object ret_obj;

      if (type_id == kTypeIdString) {
        auto str = obj.Cast<string>();
        auto type = lexical::GetStringType(str, true);

        switch (type) {
        case kStringTypeInt:
          ret_obj.PackContent(make_shared<int64_t>(stol(str)), kTypeIdInt);
          break;
        case kStringTypeFloat:
          ret_obj.PackContent(make_shared<double>(stod(str)), kTypeIdFloat);
          break;
        case kStringTypeBool:
          ret_obj.PackContent(make_shared<bool>(str == kStrTrue), kTypeIdBool);
          break;
        default:
          ret_obj = obj;
          break;
        }
      }
      else {
        if (!type::CheckMethod(kStrGetStr, type_id)) {
          frame.MakeError("Invalid argument for convert()");
          return;
        }

        auto ret_obj = Invoke(obj, kStrGetStr).GetObj();
        if (frame.error) return;
      }

      frame.RefreshReturnStack(ret_obj);
    }
  }

  void Machine::CommandUsing(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - load(obj)");
      return;
    }

    auto path_obj = FetchObject(args[0]);

    if (path_obj.GetTypeId() != kTypeIdString) {
      frame.MakeError("Invalid path");
      return;
    }

    string path = path_obj.Cast<string>();
    fs::path path_cls(path);
    string extension_name = lexical::ToLower(path_cls.extension().string());

    if (extension_name == ".kagami") {
      VMCode &script_file = management::script::AppendBlankScript(path);

      if (!script_file.empty()) return;

      VMCodeFactory factory(path, script_file, logger_);

      if (factory.Start()) {
        Machine sub_machine(script_file, logger_);
        auto &obj_base = obj_stack_.GetBase();
        sub_machine.SetDelegatedRoot(obj_base.front());
        sub_machine.Run();

        if (sub_machine.ErrorOccurred()) {
          frame.MakeError("Error is occurred in loaded script");
        }
      }
      else {
        frame.MakeError("Invalid script - " + path);
      }
    }
    else if (extension_name == ".toml") {
      ConfigProcessor config_proc(obj_stack_, frame_stack_, path_obj.Cast<string>());
      if (frame.error) return;
      config_proc.InitWindowFromConfig();
    }
  }

  void Machine::CommandUsingTable(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(2)) {
      frame.MakeError("Argument is misssing - using_table(obj, obj)");
      return;
    }

    auto window_obj = FetchObject(args[1]);
    auto path_obj = FetchObject(args[0]);
    
    if (path_obj.GetTypeId() != kTypeIdString) {
      frame.MakeError("Invalid table file path");
      return;
    }

    if (window_obj.GetTypeId() != kTypeIdWindow) {
      frame.MakeError("Invalid window object");
    }

    auto &window = window_obj.Cast<dawn::PlainWindow>();
    ConfigProcessor config_proc(obj_stack_, frame_stack_, path_obj.Cast<string>());
    if (frame.error) return;
    auto managed_table = make_shared<ObjectTable>();
    Object table_obj(managed_table, kTypeIdTable);

    string variant_string = config_proc.GetTableVariant();
    if (frame.error) return;

    if (variant_string == "texture") {
      config_proc.InitTextureTable(*managed_table, window);
    }
    else if (variant_string == "rectangle") {
      config_proc.InitRectangleTable(*managed_table);
    }
    
    if (frame.error) return;
    frame.RefreshReturnStack(table_obj);
  }

  void Machine::CommandApplyLayout(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(2)) {
      frame.MakeError("Argument is misssing - apply_layout(obj, obj)");
      return;
    }

    auto window_obj = FetchObject(args[1]);
    auto path_obj = FetchObject(args[0]);

    if (path_obj.GetTypeId() != kTypeIdString) {
      frame.MakeError("Invalid table file path");
      return;
    }

    if (window_obj.GetTypeId() != kTypeIdWindow) {
      frame.MakeError("Invalid window object");
    }

    auto &window = window_obj.Cast<dawn::PlainWindow>();
    ConfigProcessor config_proc(obj_stack_, frame_stack_, path_obj.Cast<string>());
    if (frame.error) return;

    config_proc.ApplyInterfaceLayout(window);
  }

  void Machine::CommandOffensiveMode(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is misssing - offensive_mode(obj, obj)");
      return;
    }

    auto value_obj = FetchObject(args[0]);

    if (value_obj.GetTypeId() != kTypeIdBool) {
      frame.MakeError("Invalid boolean value");
    }

    offensive_ = value_obj.Cast<bool>();
  }

  void Machine::CommandTime() {
    auto &frame = frame_stack_.top();
    time_t now = time(nullptr);
    string nowtime(ctime(&now));
    nowtime.pop_back();
    frame.RefreshReturnStack(Object(nowtime));
  }

  void Machine::CommandVersion() {
    auto &frame = frame_stack_.top();
    frame.RefreshReturnStack(Object(PRODUCT_VER));
  }

  void Machine::CommandMachineCodeName() {
    auto &frame = frame_stack_.top();
    frame.RefreshReturnStack(Object(CODENAME));
  }

  template <Keyword op_code>
  void Machine::BinaryMathOperatorImpl(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(2)) {
      frame.MakeError("Argument behind operator is missing");
      return;
    }

    auto rhs = FetchObject(args[1]);
    auto lhs = FetchObject(args[0]);
    auto type_rhs = FindTypeCode(rhs.GetTypeId());
    auto type_lhs = FindTypeCode(lhs.GetTypeId());

    if (frame.error) return;

    if (type_lhs == kNotPlainType || type_rhs == kNotPlainType) {
      frame.MakeError("Try to operate with non-plain type.");
      return;
    }

    auto result_type = kResultDynamicTraits.at(ResultTraitKey(type_lhs, type_rhs));

#define RESULT_PROCESSING(_Type, _Func, _TypeId)                       \
  _Type result = MathBox<_Type, op_code>().Do(_Func(lhs), _Func(rhs)); \
  frame.RefreshReturnStack(Object(result, _TypeId));

    if (result_type == kPlainString) {
      if (IsIllegalStringOperator(op_code)) {
        frame.RefreshReturnStack();
        return;
      }

      RESULT_PROCESSING(string, StringProducer, kTypeIdString);
    }
    else if (result_type == kPlainInt) {
      RESULT_PROCESSING(int64_t, IntProducer, kTypeIdInt);
    }
    else if (result_type == kPlainFloat) {
      RESULT_PROCESSING(double, FloatProducer, kTypeIdFloat);
    }
    else if (result_type == kPlainBool) {
      RESULT_PROCESSING(bool, BoolProducer, kTypeIdBool);
    }
#undef RESULT_PROCESSING
  }

  template <Keyword op_code>
  void Machine::BinaryLogicOperatorImpl(ArgumentList &args) {
    using namespace type;
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(2)) {
      frame.MakeError("Argument behind operator is missing");
      return;
    }

    auto rhs = FetchObject(args[1]);
    auto lhs = FetchObject(args[0]);
    auto type_rhs = FindTypeCode(rhs.GetTypeId());
    auto type_lhs = FindTypeCode(lhs.GetTypeId());
    bool result = false;

    if (frame.error) return;

    if (!lexical::IsPlainType(lhs.GetTypeId())) {
      if (op_code != kKeywordEquals && op_code != kKeywordNotEqual) {
        frame.RefreshReturnStack();
        return;
      }

      if (!CheckMethod(kStrCompare, lhs.GetTypeId())) {
        frame.MakeError("Can't operate with this operator.");
        return;
      }

      Object obj = Invoke(lhs, kStrCompare,
        { NamedObject(kStrRightHandSide, rhs) }).GetObj();
      if (frame.error) return;

      if (obj.GetTypeId() != kTypeIdBool) {
        frame.MakeError("Invalid behavior of compare().");
        return;
      }

      if (op_code == kKeywordNotEqual) {
        bool value = !obj.Cast<bool>();
        frame.RefreshReturnStack(Object(value, kTypeIdBool));
      }
      else {
        frame.RefreshReturnStack(obj);
      }
      return;
    }

    auto result_type = kResultDynamicTraits.at(ResultTraitKey(type_lhs, type_rhs));
#define RESULT_PROCESSING(_Type, _Func)\
  result = LogicBox<_Type, op_code>().Do(_Func(lhs), _Func(rhs));

    if (result_type == kPlainString) {
      if (IsIllegalStringOperator(op_code)) {
        frame.RefreshReturnStack();
        return;
      }

      RESULT_PROCESSING(string, StringProducer);
    }
    else if (result_type == kPlainInt) {
      RESULT_PROCESSING(int64_t, IntProducer);
    }
    else if (result_type == kPlainFloat) {
      RESULT_PROCESSING(double, FloatProducer);
    }
    else if (result_type == kPlainBool) {
      RESULT_PROCESSING(bool, BoolProducer);
    }

    frame.RefreshReturnStack(Object(result, kTypeIdBool));
#undef RESULT_PROCESSING
  }

  void Machine::OperatorLogicNot(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument behind operator is missing");
      return;
    }

    auto rhs = FetchObject(args[0]);

    if (rhs.GetTypeId() != kTypeIdBool) {
      frame.MakeError("Can't operate with this operator");
      return;
    }

    bool result = !rhs.Cast<bool>();

    frame.RefreshReturnStack(Object(result, kTypeIdBool));
  }


  void Machine::ExpList(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    if (!args.empty()) {
      frame.RefreshReturnStack(FetchObject(args.back()));
    }
  }

  void Machine::InitArray(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    ManagedArray base = make_shared<ObjectArray>();

    if (!args.empty()) {
      for (auto &unit : args) {
        base->emplace_back(FetchObject(unit));
      }
    }

    Object obj(base, kTypeIdArray);
    frame.RefreshReturnStack(obj);
  }

  void Machine::CommandReturn(ArgumentList &args) {
    if (frame_stack_.size() == 1) {
      frame_stack_.top().MakeError("Unexpected return");
      return;
    }

    bool dispose_return_value = frame_stack_.top().event_processing;

    if (args.size() == 1) {
      Object ret_obj = FetchObject(args[0]).Unpack();

      auto *container = &obj_stack_.GetCurrent();
      while (container->Find(kStrUserFunc) == nullptr) {
        obj_stack_.Pop();
        container = &obj_stack_.GetCurrent();
      }

      RecoverLastState();
      if (!dispose_return_value) {
        frame_stack_.top().RefreshReturnStack(ret_obj);
      }
    }
    else if (args.size() == 0) {
      auto *container = &obj_stack_.GetCurrent();
      while (container->Find(kStrUserFunc) == nullptr) {
        obj_stack_.Pop();
        container = &obj_stack_.GetCurrent();
      }

      RecoverLastState();
      if (!dispose_return_value) {
        frame_stack_.top().RefreshReturnStack(Object());
      }
    }
    else {
      ManagedArray obj_array = make_shared<ObjectArray>();
      for (auto it = args.begin(); it != args.end(); ++it) {
        obj_array->emplace_back(FetchObject(*it).Unpack());
      }
      Object ret_obj(obj_array, kTypeIdArray);

      auto *container = &obj_stack_.GetCurrent();
      while (container->Find(kStrUserFunc) == nullptr) {
        obj_stack_.Pop();
        container = &obj_stack_.GetCurrent();
      }

      RecoverLastState();
      if (!dispose_return_value) {
        frame_stack_.top().RefreshReturnStack(ret_obj);
      }
    }
  }

  void Machine::CommandAssert(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(1)) {
      frame.MakeError("Argument is missing  - assert(bool_obj)");
      return;
    }

    auto result_obj = FetchObject(args[0]);

    if (result_obj.GetTypeId() != kTypeIdBool) {
      frame.MakeError("Invalid object type for assertion.");
      return;
    }

    if (!result_obj.Cast<bool>()) {
      frame.MakeError("User assertion failed.");
    }
  }

  void Machine::CommandHandle(ArgumentList &args) {
    auto &frame = frame_stack_.top();

    if (!EXPECTED_COUNT(3)) {
      frame.MakeError("Argument is missing  - handle(win, event, func)");
      return;
    }

    auto func = FetchObject(args[2]);
    auto event_type_obj = FetchObject(args[1]);
    auto window_obj = FetchObject(args[0]);

    auto window_id = window_obj.Cast<dawn::PlainWindow>().GetId();
    auto event_type = static_cast<Uint32>(event_type_obj.Cast<int64_t>());
    EventHandlerMark handler(window_id, event_type);

    auto it = event_list_.find(handler);

    if (it != event_list_.end()) {
      auto &func_impl = func.Cast<FunctionImpl>();
      it->second = func_impl;
    }
    else {
      auto &func_impl = func.Cast<FunctionImpl>();
      auto dest = make_pair(EventHandlerMark(window_id, event_type), func_impl);
      event_list_.insert(dest);
    }
  }

  void Machine::CommandWait(ArgumentList &args) {
    hanging_ = true;
  }

  void Machine::CommandLeave(ArgumentList &args) {
    hanging_ = false;
  }

  void Machine::DomainAssert(ArgumentList &args) {
    auto &frame = frame_stack_.top();
    frame.assert_rc_copy = FetchObject(args[0]).Unpack();
  }

  void Machine::MachineCommands(Keyword token, ArgumentList &args, Request &request) {
    auto &frame = frame_stack_.top();

    switch (token) {
    case kKeywordPlus:
      BinaryMathOperatorImpl<kKeywordPlus>(args);
      break;
    case kKeywordMinus:
      BinaryMathOperatorImpl<kKeywordMinus>(args);
      break;
    case kKeywordTimes:
      BinaryMathOperatorImpl<kKeywordTimes>(args);
      break;
    case kKeywordDivide:
      BinaryMathOperatorImpl<kKeywordDivide>(args);
      break;
    case kKeywordEquals:
      BinaryLogicOperatorImpl<kKeywordEquals>(args);
      break;
    case kKeywordLessOrEqual:
      BinaryLogicOperatorImpl<kKeywordLessOrEqual>(args);
      break;
    case kKeywordGreaterOrEqual:
      BinaryLogicOperatorImpl<kKeywordGreaterOrEqual>(args);
      break;
    case kKeywordNotEqual:
      BinaryLogicOperatorImpl<kKeywordNotEqual>(args);
      break;
    case kKeywordGreater:
      BinaryLogicOperatorImpl<kKeywordGreater>(args);
      break;
    case kKeywordLess:
      BinaryLogicOperatorImpl<kKeywordLess>(args);
      break;
    case kKeywordAnd:
      BinaryLogicOperatorImpl<kKeywordAnd>(args);
      break;
    case kKeywordOr:
      BinaryLogicOperatorImpl<kKeywordOr>(args);
      break;
    case kKeywordNot:
      OperatorLogicNot(args);
      break;
    case kKeywordHash:
      CommandHash(args);
      break;
    case kKeywordFor:
      CommandForEach(args, request.option.nest_end);
      break;
    case kKeywordNullObj:
      CommandNullObj(args);
      break;
    case kKeywordDestroy:
      CommandDestroy(args);
      break;
    case kKeywordConvert:
      CommandConvert(args);
      break;
    case kKeywordTime:
      CommandTime();
      break;
    case kKeywordVersion:
      CommandVersion();
      break;
    case kKeywordCodeName:
      CommandMachineCodeName();
      break;
    case kKeywordSwap:
      CommandSwap(args);
      break;
    case kKeywordBind:
      CommandBind(args, request.option.local_object,
        request.option.ext_object);
      break;
    case kKeywordDelivering:
      CommandDelivering(args, request.option.local_object,
        request.option.ext_object);
      break;
    case kKeywordExpList:
      ExpList(args);
      break;
    case kKeywordInitialArray:
      InitArray(args);
      break;
    case kKeywordReturn:
      CommandReturn(args);
      break;
    case kKeywordAssert:
      CommandAssert(args);
      break;
    case kKeywordTypeId:
      CommandTypeId(args);
      break;
    case kKeywordMethods:
      CommandMethods(args);
      break;
    case kKeywordExist:
      CommandExist(args);
      break;
    case kKeywordFn:
      ClosureCatching(args, request.option.nest_end, frame_stack_.size() > 1);
      break;
    case kKeywordCase:
      CommandCase(args, request.option.nest_end);
      break;
    case kKeywordWhen:
      CommandWhen(args);
      break;
    case kKeywordEnd:
      switch (request.option.nest_root) {
      case kKeywordWhile:
        CommandLoopEnd(request.option.nest);
        break;
      case kKeywordFor:
        CommandForEachEnd(request.option.nest);
        break;
      case kKeywordIf:
      case kKeywordCase:
        CommandConditionEnd();
        break;
      case kKeywordStruct:
        CommandStructEnd();
        break;
      case kKeywordModule:
        CommandModuleEnd();
        break;
      default:break;
      }
      break;
    case kKeywordContinue:
    case kKeywordBreak:
      CommandContinueOrBreak(token, request.option.escape_depth);
      break;
    case kKeywordElse:
      CommandElse();
      break;
    case kKeywordIf:
    case kKeywordElif:
    case kKeywordWhile:
      CommandIfOrWhile(token, args, request.option.nest_end);
      break;
    case kKeywordHandle:
      CommandHandle(args);
      break;
    case kKeywordWait:
      CommandWait(args);
      break;
    case kKeywordLeave:
      CommandLeave(args);
      break;
    case kKeywordUsing:
      CommandUsing(args);
      break;
    case kKeywordUsingTable:
      CommandUsingTable(args);
      break;
    case kKeywordApplyLayout:
      CommandApplyLayout(args);
      break;
    case kKeywordOffensiveMode:
      CommandOffensiveMode(args);
      break;
    case kKeywordStruct:
      CommandStructBegin(args);
      break;
    case kKeywordModule:
      CommandModuleBegin(args);
      break;
    case kKeywordDomainAssertCommand:
      DomainAssert(args);
      break;
    case kKeywordInclude:
      CommandInclude(args);
      break;
      //Super
    default:
      break;
    }
  }

  void Machine::GenerateArgs(FunctionImpl &impl, ArgumentList &args, ObjectMap &obj_map) {
    switch (impl.GetPattern()) {
    case kParamFixed:
      Generate_Fixed(impl, args, obj_map);
      break;
    case kParamAutoSize:
      Generate_AutoSize(impl, args, obj_map);
      break;
    case kParamAutoFill:
      Generate_AutoFill(impl, args, obj_map);
      break;
    default:
      break;
    }
  }

  void Machine::Generate_Fixed(FunctionImpl &impl, ArgumentList &args, ObjectMap &obj_map) {
    auto &frame = frame_stack_.top();
    auto &params = impl.GetParameters();
    size_t pos = args.size() - 1;

    if (args.size() > params.size()) {
      frame.MakeError("Too many arguments");
      return;
    }

    if (args.size() < params.size()) {
      frame.MakeError("Minimum argument amount is " + to_string(params.size()));
      return;
    }


    for (auto it = params.rbegin(); it != params.rend(); ++it) {
      obj_map.emplace(NamedObject(*it, FetchObject(args[pos]).RemoveDeliveringFlag()));
      pos -= 1;
    }
  }

  void Machine::Generate_AutoSize(FunctionImpl &impl, ArgumentList &args, ObjectMap &obj_map) {
    auto &frame = frame_stack_.top();
    vector<string> &params = impl.GetParameters();
    list<Object> temp_list;
    ManagedArray va_base = make_shared<ObjectArray>();
    size_t pos = args.size(), diff = args.size() - params.size() + 1;

    if (args.size() < params.size()) {
      frame.MakeError("Minimum argument amount is " + to_string(params.size()));
      return;
    }

    while (diff != 0) {
      temp_list.emplace_front(FetchObject(args[pos - 1]).RemoveDeliveringFlag());
      pos -= 1;
      diff -= 1;
    }

    if (!temp_list.empty()) {
      for (auto it = temp_list.begin(); it != temp_list.end(); ++it) {
        va_base->emplace_back(*it);
      }

      temp_list.clear();
    }

    obj_map.insert(NamedObject(params.back(), Object(va_base, kTypeIdArray)));


    while (pos > 0) {
      obj_map.emplace(params[pos - 1], FetchObject(args[pos - 1]).RemoveDeliveringFlag());
      pos -= 1;
    }
  }

  void Machine::Generate_AutoFill(FunctionImpl &impl, ArgumentList &args, ObjectMap &obj_map) {
    auto &frame = frame_stack_.top();
    auto &params = impl.GetParameters();
    size_t limit = impl.GetLimit();
    size_t pos = args.size() - 1, param_pos = params.size() - 1;

    if (args.size() > params.size()) {
      frame.MakeError("Too many arguments");
      return;
    }

    if (args.size() < limit) {
      frame.MakeError("Minimum argument amount is " + to_string(limit));
      return;
    }

    for (auto it = params.crbegin(); it != params.crend(); ++it) {
      if (param_pos != pos) {
        obj_map.emplace(NamedObject(*it, Object()));
      }
      else {
        obj_map.emplace(NamedObject(*it, FetchObject(args[pos]).RemoveDeliveringFlag()));
        pos -= 1;
      }
      param_pos -= 1;
    }
  }

  void Machine::LoadEventInfo(SDL_Event &event, ObjectMap &obj_map, FunctionImpl &impl, Uint32 id) {
    auto &frame = frame_stack_.top();
    auto window = dynamic_cast<dawn::PlainWindow *>(dawn::GetWindowById(id));
    Object this_window_obj(window, kTypeIdWindow);
    obj_map.insert(NamedObject(kStrThisWindow, this_window_obj));

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      if (impl.GetParamSize() != 1) {
        frame.MakeError("Invalid function for KeyDown/KeyUp event");
        return;
      }
      auto &params = impl.GetParameters();
      int64_t keysym = static_cast<int64_t>(event.key.keysym.sym);
      obj_map.insert(NamedObject(params[0], Object(keysym, kTypeIdInt)));
    }

    if (event.type == SDL_WINDOWEVENT) {
      if (impl.GetParamSize() != 1) {
        frame.MakeError("Invalid function for window event");
        return;
      }
      auto &params = impl.GetParameters();
      obj_map.insert(NamedObject(params[0], Object(event.window, kTypeIdWindowEvent)));
    }

    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
      if (impl.GetParamSize() != 3) {
        frame.MakeError("Invalid function for MouseDown/MouseUp event");
        return;
      }
      auto &params = impl.GetParameters();
      auto y = static_cast<int64_t>(event.button.y);
      auto x = static_cast<int64_t>(event.button.x);
      auto button = static_cast<int64_t>(event.button.button);
      obj_map.insert(NamedObject(params[2], Object(y, kTypeIdInt)));
      obj_map.insert(NamedObject(params[1], Object(x, kTypeIdInt)));
      obj_map.insert(NamedObject(params[0], Object(button, kTypeIdInt)));
    }

    if (event.type == SDL_MOUSEMOTION) {
      if (impl.GetParamSize() != 4) {
        frame.MakeError("Invalid function for MouseMotion event");
        return;
      }
      auto &params = impl.GetParameters();
      auto yrel = static_cast<int64_t>(event.motion.yrel);
      auto xrel = static_cast<int64_t>(event.motion.xrel);
      auto y = static_cast<int64_t>(event.motion.y);
      auto x = static_cast<int64_t>(event.motion.x);
      obj_map.insert(NamedObject(params[3], Object(yrel, kTypeIdInt)));
      obj_map.insert(NamedObject(params[2], Object(xrel, kTypeIdInt)));
      obj_map.insert(NamedObject(params[1], Object(y, kTypeIdInt)));
      obj_map.insert(NamedObject(params[0], Object(x, kTypeIdInt)));
    }
  }

  void Machine::CallExtensionFunction(ObjectMap &p, FunctionImpl &impl) {
    auto &frame = frame_stack_.top();
    Object returning_slot;
    auto ext_activity = impl.GetExtActivity();
    VMState vm_state{ &p, &returning_slot, this, ReceiveExtReturningValue };
    auto result_code = ext_activity(vm_state);
    if (result_code < 1) {
      frame.MakeError("Extension reports error while invoking external activity.");
      return;
    }
    frame.RefreshReturnStack(returning_slot);
  }

  void Machine::GenerateStructInstance(ObjectMap &p) {
    using namespace type;
    auto &frame = frame_stack_.top();

    auto &base = frame.struct_base.Cast<ObjectStruct>().GetContent();
    auto managed_instance = make_shared<ObjectStruct>();
    auto struct_id = base.at(kStrStructId).Cast<string>();

    for (auto &unit : base) {
      if (unit.first == kStrInitializer) continue;
      if (unit.first == kStrStructId) continue;

      //create new object copy instead of RC copy
      managed_instance->Add(unit.first, CreateObjectCopy(unit.second));
    }

    Object instance_obj(managed_instance, struct_id);
    instance_obj.SetContainerFlag();
    p.insert(NamedObject(kStrMe, instance_obj));
    frame.struct_base = Object();
  }

  //for extension callback facilities
  bool Machine::PushObject(string id, Object object) {
    auto &frame = frame_stack_.top();
    auto result = obj_stack_.CreateObject(id, object);
    if (!result) {
      frame.MakeError("Cannot create object");
      return false;
    }
    return true;
  }

  void Machine::PushError(string msg) {
    auto &frame = frame_stack_.top();
    frame.MakeError(msg);
  }

  void Machine::Run(bool invoking, string id, VMCodePointer ptr, ObjectMap *p,
    ObjectMap *closure_record) {
    if (code_stack_.empty()) return;
    if (invoking) code_stack_.push_back(ptr);

    bool interface_error = false;
    bool invoking_error = false;
    size_t stop_point = invoking ? frame_stack_.size() : 0;
    size_t script_idx = 0;
    Message msg;
    VMCode *code = code_stack_.back();
    Command *command = nullptr;
    FunctionImplPointer impl;
    ObjectMap obj_map;
    SDL_Event event;

    frame_stack_.push(RuntimeFrame());
    obj_stack_.Push();

    if (invoking) {
      obj_stack_.CreateObject(kStrUserFunc, Object(id));
      obj_stack_.MergeMap(*p);
      obj_stack_.MergeMap(*closure_record);
      frame_stack_.top().function_scope = id;
    }

    RuntimeFrame *frame = &frame_stack_.top();
    size_t size = code->size();

    //Refreshing loop tick state to make it work correctly.
    auto refresh_tick = [&]() -> void {
      code = code_stack_.back();
      size = code->size();
      frame = &frame_stack_.top();
    };

    //Protect current runtime environment and load another function
    auto update_stack_frame = [&](FunctionImpl &func) -> void {
      //block other event trigger while processing current event function
      bool event_processing = frame->event_processing;
      bool inside_initializer_calling = frame->initializer_calling;
      frame->initializer_calling = false;
      code_stack_.push_back(&func.GetCode());
      frame_stack_.push(RuntimeFrame(func.GetId()));
      obj_stack_.Push();
      obj_stack_.CreateObject(kStrUserFunc, Object(func.GetId()));
      obj_stack_.MergeMap(obj_map);
      obj_stack_.MergeMap(impl->GetClosureRecord());
      refresh_tick();
      frame->jump_offset = func.GetOffset();
      frame->event_processing = event_processing;
      frame->inside_initializer_calling = inside_initializer_calling;
    };

    //Convert current environment to next self-calling 
    auto tail_recursion = [&]() -> void {
      bool event_processing = frame->event_processing;
      string function_scope = frame_stack_.top().function_scope;
      size_t jump_offset = frame_stack_.top().jump_offset;
      obj_map.Naturalize(obj_stack_.GetCurrent());
      frame_stack_.top() = RuntimeFrame(function_scope);
      obj_stack_.ClearCurrent();
      obj_stack_.CreateObject(kStrUserFunc, Object(function_scope));
      obj_stack_.MergeMap(obj_map);
      obj_stack_.MergeMap(impl->GetClosureRecord());
      refresh_tick();
      frame->jump_offset = jump_offset;
      frame->event_processing = event_processing;
    };

    //Convert current environment to next calling
    auto tail_call = [&](FunctionImpl &func) -> void {
      bool event_processing = frame->event_processing;
      code_stack_.pop_back();
      code_stack_.push_back(&func.GetCode());
      obj_map.Naturalize(obj_stack_.GetCurrent());
      frame_stack_.top() = RuntimeFrame(func.GetId());
      obj_stack_.ClearCurrent();
      obj_stack_.CreateObject(kStrUserFunc, Object(func.GetId()));
      obj_stack_.MergeMap(obj_map);
      obj_stack_.MergeMap(impl->GetClosureRecord());
      refresh_tick();
      frame->jump_offset = func.GetOffset();
      frame->event_processing = event_processing;
    };

    // Main loop of virtual machine.
    // TODO:dispose return value in event function
    while (frame->idx < size || frame_stack_.size() > 1 || hanging_) {
      //freeze mainloop to keep querying events
      freezing_ = (frame->idx >= size && hanging_ && frame_stack_.size() == 1);

      if (frame->warning) {
        AppendMessage(frame->msg_string, kStateWarning, logger_);
        frame->warning = false;
      }

      //stop at invoking point
      if (invoking && frame_stack_.size() == stop_point) {
        break;
      }

      //Draw all windows
      if (offensive_) dawn::ForceRefreshingAllWindow();

      //window event handler
      //cannot invoke new event inside a running event function
      if ((hanging_ && !frame->event_processing && SDL_PollEvent(&event) != 0) 
        || (freezing_ && SDL_WaitEvent(&event) != 0)) {
        EventHandlerMark mark(event.window.windowID, event.type);
        auto it = event_list_.find(mark);
        if (it != event_list_.end()) {
          obj_map.clear();
          LoadEventInfo(event, obj_map, it->second, event.window.windowID);

          if (frame->error) break;

          update_stack_frame(it->second);
          refresh_tick();
          frame->event_processing = true;
          continue;
        }

        if (freezing_) continue;
      }

      //switch to last stack frame when indicator reaches end of the block.
      //return expression will be processed in Machine::CommandReturn
      if (frame->idx == size && frame_stack_.size() > 1) {
        //Bring saved environment back
        if (frame->inside_initializer_calling) FinishInitalizerCalling();
        else RecoverLastState();
        //Update register data
        refresh_tick();
        if (!freezing_) {
          frame->Stepping();
        }
        continue;
      }

      //load current command and refreshing indicators
      command = &(*code)[frame->idx];
      script_idx = command->first.idx;
      // dispose returning value
      frame->void_call = command->first.option.void_call; 

      //Built-in machine commands.
      if (command->first.type == kRequestCommand) {
        MachineCommands(command->first.GetKeywordValue(), command->second, command->first);
        
        if (command->first.GetKeywordValue() == kKeywordReturn) refresh_tick();

        if (frame->error) {
          script_idx = command->first.idx;
          break;
        }

        frame->Stepping();
        continue;
      }

      //cleaning object map for user-defined function and C++ function
      obj_map.clear();

      //Query function(Interpreter built-in or user-defined)
      //error string will be generated in FetchFunctionImpl.
      if (command->first.type == kRequestFunction) {
        if (!FetchFunctionImpl(impl, command, obj_map)) break;
      }

      //Build object map for function call expressed by command
      GenerateArgs(*impl, command->second, obj_map);

      if (frame->initializer_calling) {
        GenerateStructInstance(obj_map);
      }

      if (frame->error) {
        //Get actual script index for error reporting
        script_idx = command->first.idx;
        break;
      }

      //user-defined function invoking
      //Ceate new stack frame and push VMCode pointer to machine stack,
      //and start new processing in next tick.
      if (impl->GetType() == kFunctionVMCode) {
        if (IsTailRecursion(frame->idx, &impl->GetCode())) tail_recursion();
        else if (IsTailCall(frame->idx)) tail_call(*impl);
        else update_stack_frame(*impl);

        continue;
      }
      else if (impl->GetType() == kFunctionExternal) {
        CallExtensionFunction(obj_map, *impl);

        if (frame->error) {
          //Get actual script index for error reporting
          script_idx = command->first.idx;
          break;
        }

        frame->Stepping();
        continue;
      }
      else if (impl->GetType() == kFunctionCXX) {
        //calling C++ functions.
        msg = impl->GetActivity()(obj_map);

        if (msg.GetLevel() == kStateError) {
          interface_error = true;
          break;
        }
      }

      //Invoke by return value.
      if (msg.IsInvokingMsg()) {
        //process invoking request in returning message
        auto invoking_req = BuildStringVector(msg.GetDetail());
        if (!_FetchFunctionImpl(impl, invoking_req[0], invoking_req[1])) {
          break;
        }

        //not checked.for OOP feature in the future.
        if (impl->GetType() == kFunctionVMCode) {
          update_stack_frame(*impl);
        }
        else {
          //calling method.
          msg = impl->GetActivity()(obj_map);
          frame->Stepping();
        }
        continue;
      }
      
      //Pushing returning value to returning stack.
      frame->RefreshReturnStack(msg.GetObj());
      //indicator + 1
      frame->Stepping();
    }

    if (frame->error) {
      AppendMessage(frame->msg_string, kStateError,
        logger_, script_idx);
      if (invoking) invoking_error = true;
    }

    if (interface_error) {
      AppendMessage(msg.GetDetail(), msg.GetLevel(), logger_, script_idx);
      if (invoking) invoking_error = true;
    }

    if (!invoking || (invoking && frame_stack_.size() != stop_point)) {
      obj_stack_.Pop();
      frame_stack_.pop();
      code_stack_.pop_back();
    }

    if (invoking && invoking_error) {
      frame_stack_.top().MakeError("Invoking error is occurred.");
    }

    error_ = frame->error || interface_error || invoking_error;
  }
}
