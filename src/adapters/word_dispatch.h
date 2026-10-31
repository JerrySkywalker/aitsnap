// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#include <QString>
#include <QDir>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace aitsnap::word {
inline void require(HRESULT hr, const char* operation)
{
    if (FAILED(hr)) throw std::runtime_error(std::string(operation) + ": HRESULT=" + QString::number(quint32(hr), 16).toStdString());
}
class Object;
class Value {
public:
    VARIANT value;
    Value() { VariantInit(&value); }
    explicit Value(int n) : Value() { value.vt = VT_I4; value.lVal = n; }
    explicit Value(double n) : Value() { value.vt = VT_R8; value.dblVal = n; }
    explicit Value(bool b) : Value() { value.vt = VT_BOOL; value.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE; }
    Value(const char*) = delete; // Require QStringLiteral/QString; a pointer must not silently become VT_BOOL.
    explicit Value(const QString& s) : Value() { value.vt = VT_BSTR; value.bstrVal = SysAllocStringLen(reinterpret_cast<const OLECHAR*>(s.utf16()), UINT(s.size())); }
    explicit Value(IDispatch* p) : Value() { value.vt = VT_DISPATCH; value.pdispVal = p; if (p) p->AddRef(); }
    Value(const Value& other) : Value() { require(VariantCopy(&value, const_cast<VARIANT*>(&other.value)), "VariantCopy"); }
    Value(Value&& other) noexcept : value(other.value) { VariantInit(&other.value); }
    ~Value() { VariantClear(&value); }
    Value& operator=(Value other) { std::swap(value, other.value); return *this; }
    int integer() const { Value converted; require(VariantChangeType(&converted.value, const_cast<VARIANT*>(&value), 0, VT_I4), "integer conversion"); return converted.value.lVal; }
    double number() const { Value converted; require(VariantChangeType(&converted.value, const_cast<VARIANT*>(&value), 0, VT_R8), "number conversion"); return converted.value.dblVal; }
    QString text() const { Value converted; require(VariantChangeType(&converted.value, const_cast<VARIANT*>(&value), 0, VT_BSTR), "text conversion"); return QString::fromWCharArray(converted.value.bstrVal); }
    Object object() const;
};
class Object {
public:
    Object() = default;
    explicit Object(IDispatch* p, bool addRef = false) : ptr_(p, [](IDispatch* q) { if (q) q->Release(); }) { if (addRef && p) p->AddRef(); }
    explicit operator bool() const { return bool(ptr_); }
    IDispatch* raw() const { return ptr_.get(); }
    Value invoke(const wchar_t* name, WORD flags, const std::vector<Value>& args = {}) const
    {
        if (!ptr_) throw std::runtime_error("Null Word COM object");
        DISPID member{}; auto mutableName = const_cast<LPOLESTR>(name);
        require(ptr_->GetIDsOfNames(IID_NULL, &mutableName, 1, LOCALE_USER_DEFAULT, &member), "Word member lookup");
        struct Arguments { std::vector<VARIANTARG> data; ~Arguments() { for (auto& v : data) VariantClear(&v); } } reversed;
        reversed.data.resize(args.size());
        for (std::size_t i = 0; i < args.size(); ++i) {
            VariantInit(&reversed.data[i]); require(VariantCopy(&reversed.data[i], const_cast<VARIANT*>(&args[args.size() - i - 1].value)), "Word argument copy");
        }
        DISPID put = DISPID_PROPERTYPUT;
        DISPPARAMS params{}; params.rgvarg = reversed.data.data(); params.cArgs = UINT(args.size());
        if (flags & DISPATCH_PROPERTYPUT) { params.rgdispidNamedArgs = &put; params.cNamedArgs = 1; }
        EXCEPINFO exception{}; UINT argError{}; Value result;
        HRESULT hr = ptr_->Invoke(member, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, &result.value, &exception, &argError);
        const auto detail = exception.bstrDescription ? QString::fromWCharArray(exception.bstrDescription).toStdString() : std::string{};
        SysFreeString(exception.bstrSource); SysFreeString(exception.bstrDescription); SysFreeString(exception.bstrHelpFile);
        if (FAILED(hr)) throw std::runtime_error(QString::fromWCharArray(name).toStdString() + ": HRESULT=" + QString::number(quint32(hr), 16).toStdString() + " " + detail);
        return result;
    }
    Value get(const wchar_t* name) const { return invoke(name, DISPATCH_PROPERTYGET); }
    void put(const wchar_t* name, const Value& value) const { invoke(name, DISPATCH_PROPERTYPUT, {value}); }
    Value call(const wchar_t* name, const std::vector<Value>& args = {}) const { return invoke(name, DISPATCH_METHOD, args); }
private:
    std::shared_ptr<IDispatch> ptr_;
};
inline Value filePath(const QString& path) { return Value(QDir::toNativeSeparators(path)); }
inline Object Value::object() const
{
    if (value.vt != VT_DISPATCH || !value.pdispVal) throw std::runtime_error("Expected Word dispatch object; variant type=" + std::to_string(value.vt));
    return Object(value.pdispVal, true);
}
inline Object createApplication()
{
    CLSID clsid{}; require(CLSIDFromProgID(L"Word.Application", &clsid), "Word registration");
    IDispatch* p{}; require(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&p)), "Word activation");
    return Object(p);
}
} // namespace aitsnap::word
