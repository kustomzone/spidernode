// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <assert.h>

#include "v8.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "v8isolate.h"
#include "v8local.h"
#include "v8string.h"

namespace v8 {

String::Utf8Value::Utf8Value(Handle<v8::Value> obj)
  : str_(nullptr), length_(0) {
  Local<String> strVal = obj->ToString();
  JSString* str = nullptr;
  if (*strVal) {
    str = reinterpret_cast<JS::Value*>(*strVal)->toString();
  }
  if (str) {
    JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
    JSFlatString* flat = JS_FlattenString(cx, str);
    if (flat) {
      length_ = JS::GetDeflatedUTF8StringLength(flat);
      str_ = new char[length_ + 1];
      JS::DeflateStringToUTF8Buffer(flat, mozilla::RangedPtr<char>(str_, length_));
      str_[length_] = '\0';
    }
  }
}

String::Utf8Value::~Utf8Value() {
  delete[] str_;
}

String::Value::Value(Handle<v8::Value> obj)
  : str_(nullptr), length_(0) {
  Local<String> strVal = obj->ToString();
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS::UniqueTwoByteChars buffer = internal::GetFlatString(cx, strVal, &length_);
  if (buffer) {
    str_ = buffer.release();
  } else {
    length_ = 0;
  }
}

String::Value::~Value() {
  js_free(str_);
}

Local<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                  NewStringType type, int length) {
  return NewFromUtf8(isolate, data, static_cast<v8::NewStringType>(type),
                     length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                       v8::NewStringType type, int length) {
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);

  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (length > v8::String::kMaxLength) {
    return MaybeLocal<String>();
  }

  if (length < 0) {
    length = strlen(data);
  }

  size_t twoByteLen;
  JS::UniqueTwoByteChars twoByteChars(
    JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(data, length), &twoByteLen).get());
  if (!twoByteChars) {
    return MaybeLocal<String>();
  }
  JS::RootedString str(cx, JS_NewUCString(cx, twoByteChars.release(), twoByteLen));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                          v8::NewStringType type, int length) {
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString str(cx, length >= 0 ?
    JS_NewStringCopyN(cx, reinterpret_cast<const char*>(data), length) :
    JS_NewStringCopyZ(cx, reinterpret_cast<const char*>(data)));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                     NewStringType type, int length) {
  return NewFromOneByte(isolate, data, static_cast<v8::NewStringType>(type),
                        length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                          v8::NewStringType type, int length) {
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString str(cx, length >= 0 ?
    JS_NewUCStringCopyN(cx, reinterpret_cast<const char16_t*>(data), length) :
    JS_NewUCStringCopyZ(cx, reinterpret_cast<const char16_t*>(data)));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                     NewStringType type, int length) {
  return NewFromTwoByte(isolate, data, static_cast<v8::NewStringType>(type),
                        length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewExternalTwoByte(Isolate* isolate,
                                              ExternalStringResource* resource) {
  JSContext* cx = JSContextFromIsolate(isolate);

  auto fin = mozilla::MakeUnique<internal::ExternalStringFinalizer>(resource);
  if (!fin) {
    return MaybeLocal<String>();
  }

  JS::RootedString str(cx,
    JS_NewExternalString(cx, reinterpret_cast<const char16_t*>(resource->data()),
                         resource->length(), fin.release()));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewExternalOneByte(Isolate* isolate,
                                              ExternalOneByteStringResource* resource) {
  JSContext* cx = JSContextFromIsolate(isolate);

  auto fin = mozilla::MakeUnique<internal::ExternalOneByteStringFinalizer>(resource);
  if (!fin) {
    return MaybeLocal<String>();
  }

  // SpiderMonkey doesn't have a one-byte variant of JS_NewExternalString, so we
  // inflate the external string data to its two-byte equivalent.  According to
  // https://github.com/mozilla/spidernode/blob/7466b2f/deps/v8/include/v8.h#L2252-L2260
  // the external string data is immutable, so there's no risk of it changing
  // after we inflate it.  But we do need to ensure that the inflated data gets
  // deleted when the string is finalized, which FinalizeExternalString does.
  size_t length = resource->length();
  JS::UniqueTwoByteChars data(js_pod_malloc<char16_t>(length + 1));
  if (!data) {
    return MaybeLocal<String>();
  }
  const char* oneByteData = resource->data();
  for (size_t i = 0; i < length; ++i)
      data[i] = (unsigned char) oneByteData[i];
  data[length] = 0;

  JS::RootedString str(cx, JS_NewExternalString(cx, data.release(), length, fin.release()));
  if (!str) {
    return MaybeLocal<String>();
  }

  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewExternal(Isolate* isolate,
                                  ExternalStringResource* resource) {
  return NewExternalTwoByte(isolate, resource).FromMaybe(Local<String>());
}

Local<String> String::NewExternal(Isolate* isolate,
                                  ExternalOneByteStringResource* resource) {
  return NewExternalOneByte(isolate, resource).FromMaybe(Local<String>());
}

String* String::Cast(v8::Value* obj) {
  assert(reinterpret_cast<JS::Value*>(obj)->isString());
  return static_cast<String*>(obj);
}

int String::Length() const {
  JSString* thisStr = reinterpret_cast<const JS::Value*>(this)->toString();
  return JS_GetStringLength(thisStr);
}

int String::Utf8Length() const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JSString* thisStr = reinterpret_cast<const JS::Value*>(this)->toString();
  JSFlatString* flat = JS_FlattenString(cx, thisStr);
  if (!flat) {
    return 0;
  }
  return JS::GetDeflatedUTF8StringLength(flat);
}

Local<String> String::Empty(Isolate* isolate) {
  return NewFromUtf8(isolate, "");
}

Local<String> String::Concat(Handle<String> left, Handle<String> right) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString leftStr(cx, reinterpret_cast<JS::Value*>(*left)->toString());
  JS::RootedString rightStr(cx, reinterpret_cast<JS::Value*>(*right)->toString());
  JSString* result = JS_ConcatStrings(cx, leftStr, rightStr);
  if (!result) {
    return Empty(isolate);
  }
  JS::Value retVal;
  retVal.setString(result);
  return internal::Local<String>::New(isolate, retVal);
}

namespace internal {

JS::UniqueTwoByteChars GetFlatString(JSContext* cx, v8::Local<String> source, size_t* length) {
  auto sourceJsVal = reinterpret_cast<JS::Value*>(*source);
  auto sourceStr = sourceJsVal->toString();
  size_t len = JS_GetStringLength(sourceStr);
  if (length) {
    *length = len;
  }
  JS::UniqueTwoByteChars buffer(js_pod_malloc<char16_t>(len + 1));
  mozilla::Range<char16_t> dest(buffer.get(), len + 1);
  if (!JS_CopyStringChars(cx, dest, sourceStr)) {
    return nullptr;
  }
  buffer[len] = '\0';
  return buffer;
}

template<typename T>
ExternalStringFinalizerBase<T>::ExternalStringFinalizerBase(String::ExternalStringResourceBase* resource)
  : resource_(resource) {
    static_cast<T*>(this)->finalize = T::FinalizeExternalString;
  }

template<typename T>
void ExternalStringFinalizerBase<T>::dispose() {
  // Based on V8's Heap::FinalizeExternalString.

  // Dispose of the C++ object if it has not already been disposed.
  if (this->resource_ != NULL) {
    this->resource_->Dispose();
    this->resource_ = nullptr;
  }

  // Delete ourselves, which JSExternalString::finalize doesn't do,
  // presumably because it assumes we're static.
  delete this;
};

ExternalStringFinalizer::ExternalStringFinalizer(String::ExternalStringResourceBase* resource)
  : ExternalStringFinalizerBase(resource) {}

void ExternalStringFinalizer::FinalizeExternalString(const JSStringFinalizer* fin, char16_t* chars) {
  const_cast<internal::ExternalStringFinalizer*>(
    static_cast<const internal::ExternalStringFinalizer*>(fin))->dispose();
}

ExternalOneByteStringFinalizer::ExternalOneByteStringFinalizer(String::ExternalStringResourceBase* resource)
  : ExternalStringFinalizerBase(resource) {}

void ExternalOneByteStringFinalizer::FinalizeExternalString(const JSStringFinalizer* fin, char16_t* chars) {
  const_cast<internal::ExternalStringFinalizer*>(
    static_cast<const internal::ExternalStringFinalizer*>(fin))->dispose();

  // NewExternalOneByte made a two-byte copy of the data in the resource,
  // and this is that copy. The resource will handle deleting its original
  // data, but we have to delete this copy.
  delete chars;
}

}

}
