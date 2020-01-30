#include <vector>
#include <memory>
#include <napi.h>
#include "spellchecker.h"

using namespace spellchecker;

namespace {
  class Spellchecker : public Napi::ObjectWrap<Spellchecker> {
    private:
      static Napi::FunctionReference constructor;
      std::unique_ptr<SpellcheckerImplementation> impl;
      Napi::Reference<Napi::Buffer<unsigned char>> dictData;

      Napi::Value SetDictionary(const Napi::CallbackInfo &info);
      Napi::Value IsMisspelled(const Napi::CallbackInfo &info);
      Napi::Value CheckSpelling(const Napi::CallbackInfo &info);
      Napi::Value Add(const Napi::CallbackInfo &info);
      Napi::Value Remove(const Napi::CallbackInfo &info);
      Napi::Value GetAvailableDictionaries(const Napi::CallbackInfo &info);
      Napi::Value GetCorrectionsForMisspelling(const Napi::CallbackInfo &info);

    public:
      static Napi::Object Init(Napi::Env env, Napi::Object exports);

      Spellchecker(const Napi::CallbackInfo &info):
        Napi::ObjectWrap<Spellchecker>(info),
        impl(SpellcheckerFactory::CreateSpellchecker())
      {}
  };

  Napi::FunctionReference Spellchecker::constructor;

  Napi::Value Spellchecker::SetDictionary(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    bool has_contents = false;
    if (info.Length() > 1) {
      if (!info[1].IsBuffer()) {
        throw Napi::Error::New(env, "setDictionary 2nd argument must be a Buffer");
      }

      // NB: We must guarantee that we pin this Buffer
      dictData = Napi::Reference<Napi::Buffer<unsigned char>>::New(info[1].As<Napi::Buffer<unsigned char>>(), 1);
      has_contents = true;
    }

    auto language = info[0].ToString().Utf8Value();

    bool result;
    if (has_contents) {
      result = impl->SetDictionaryToContents(dictData.Value().Data(), dictData.Value().Length());
    } else {
      result = impl->SetDictionary(language);
    }

    return Napi::Boolean::New(env, result);
  }

  Napi::Value Spellchecker::IsMisspelled(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    auto word = info[0].ToString().Utf8Value();

    return Napi::Boolean::New(env, impl->IsMisspelled(word));
  }

  Napi::Value Spellchecker::CheckSpelling(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    auto utf16String = info[0].ToString().Utf16Value();
    std::vector<uint16_t> text(utf16String.length() + 1, 0);
    for (size_t i = 0; i < utf16String.length(); ++i) {
      text[i] = utf16String[i];
    }

    auto misspelled_ranges = impl->CheckSpelling(reinterpret_cast<uint16_t *>(text.data()), text.size());
    auto result = Napi::Array::New(env, misspelled_ranges.size());
    for (auto iter = misspelled_ranges.begin(); iter != misspelled_ranges.end(); ++iter) {
      size_t index = iter - misspelled_ranges.begin();
      uint32_t start = iter->start, end = iter->end;

      auto misspelled_range = Napi::Object::New(env);
      misspelled_range["start"] = Napi::Number::New(env, start);
      misspelled_range["end"] = Napi::Number::New(env, end);
      result[index] = misspelled_range;
    }

    return result;
  }

  Napi::Value Spellchecker::Add(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    impl->Add(info[0].ToString().Utf8Value());

    return env.Undefined();
  }

  Napi::Value Spellchecker::Remove(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    impl->Remove(info[0].ToString().Utf8Value());

    return env.Undefined();
  }

  Napi::Value Spellchecker::GetAvailableDictionaries(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    std::string path = ".";
    if (info.Length() > 0 && !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    if (info.Length() > 0) {
      path = info[0].ToString().Utf8Value();
    }

    auto dictionaries = impl->GetAvailableDictionaries(path);
    auto result = Napi::Array::New(env, dictionaries.size());
    for (size_t i = 0; i < dictionaries.size(); ++i) {
      auto dict = dictionaries[i];
      result[i] = Napi::String::New(env, dict);
    }

    return result;
  }

  Napi::Value Spellchecker::GetCorrectionsForMisspelling(const Napi::CallbackInfo &info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
      throw Napi::Error::New(env, "Bad argument");
    }

    auto word = info[0].ToString().Utf8Value();
    auto corrections = impl->GetCorrectionsForMisspelling(word);

    auto result = Napi::Array::New(env, corrections.size());
    for (size_t i = 0; i < corrections.size(); ++i) {
      result[i] = Napi::String::New(env, corrections[i]);
    }

    return result;
  }

  Napi::Object Spellchecker::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function spellcheckerConstructor = DefineClass(env, "Spellchecker", {
      InstanceMethod("setDictionary", &Spellchecker::SetDictionary),
      InstanceMethod("getAvailableDictionaries", &Spellchecker::GetAvailableDictionaries),
      InstanceMethod("getCorrectionsForMisspelling", &Spellchecker::GetCorrectionsForMisspelling),
      InstanceMethod("isMisspelled", &Spellchecker::IsMisspelled),
      InstanceMethod("checkSpelling", &Spellchecker::CheckSpelling),
      InstanceMethod("add", &Spellchecker::Add),
      InstanceMethod("remove", &Spellchecker::Remove)
    });

    constructor = Napi::Persistent(spellcheckerConstructor);
    constructor.SuppressDestruct();
    exports["Spellchecker"] = spellcheckerConstructor;

    return exports;
  }

  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Spellchecker::Init(env, exports);
    return exports;
  }

  NODE_API_MODULE(spellchecker, Init);
}
