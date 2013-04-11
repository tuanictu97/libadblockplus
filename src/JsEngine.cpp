#include <AdblockPlus.h>
#include <sstream>

#include "GlobalJsObject.h"

namespace
{
  v8::Handle<v8::Context> CreateContext(
    AdblockPlus::ErrorCallback& errorCallback,
    AdblockPlus::WebRequest& webRequest)
  {
    const v8::Locker locker(v8::Isolate::GetCurrent());
    const v8::HandleScope handleScope;
    const v8::Handle<v8::ObjectTemplate> global =
      AdblockPlus::GlobalJsObject::Create(errorCallback, webRequest);
    return v8::Context::New(0, global);
  }

  v8::Handle<v8::Script> CompileScript(const std::string& source, const std::string& filename)
  {
    const v8::Handle<v8::String> v8Source = v8::String::New(source.c_str());
    if (filename.length())
    {
      const v8::Handle<v8::String> v8Filename = v8::String::New(filename.c_str());
      return v8::Script::Compile(v8Source, v8Filename);
    }
    else
      return v8::Script::Compile(v8Source);
  }

  void CheckTryCatch(const v8::TryCatch& tryCatch)
  {
    if (tryCatch.HasCaught())
      throw AdblockPlus::JsError(tryCatch.Exception(), tryCatch.Message());
  }

  std::string Slurp(std::istream& stream)
  {
    std::stringstream content;
    content << stream.rdbuf();
    return content.str();
  }

  std::string ExceptionToString(const v8::Handle<v8::Value> exception,
      const v8::Handle<v8::Message> message)
  {
    std::stringstream error;
    error << *v8::String::Utf8Value(exception);
    if (!message.IsEmpty())
    {
      error << " at ";
      error << *v8::String::Utf8Value(message->GetScriptResourceName());
      error << ":";
      error << message->GetLineNumber();
    }
    return error.str();
  }
}

AdblockPlus::JsError::JsError(const v8::Handle<v8::Value> exception,
    const v8::Handle<v8::Message> message)
  : std::runtime_error(ExceptionToString(exception, message))
{
}

AdblockPlus::JsEngine::JsEngine(const FileReader* const fileReader,
                                WebRequest* const webRequest,
                                ErrorCallback* const errorCallback)
  : fileReader(fileReader), context(CreateContext(*errorCallback, *webRequest))
{
}

std::string AdblockPlus::JsEngine::Evaluate(const std::string& source,
    const std::string& filename)
{
  const v8::Locker locker(v8::Isolate::GetCurrent());
  const v8::HandleScope handleScope;
  const v8::Context::Scope contextScope(context);
  const v8::TryCatch tryCatch;
  const v8::Handle<v8::Script> script = CompileScript(source, filename);
  CheckTryCatch(tryCatch);
  v8::Local<v8::Value> result = script->Run();
  CheckTryCatch(tryCatch);
  v8::String::Utf8Value resultString(result);
  return std::string(*resultString);
}

void AdblockPlus::JsEngine::Load(const std::string& scriptPath)
{
  const std::auto_ptr<std::istream> file = fileReader->Read(scriptPath);
  if (!*file)
    throw std::runtime_error("Unable to load script " + scriptPath);
  Evaluate(Slurp(*file));
}

void AdblockPlus::JsEngine::Gc()
{
  while (!v8::V8::IdleNotification());
}
