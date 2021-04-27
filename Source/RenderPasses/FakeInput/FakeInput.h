#pragma once
#include "Falcor.h"

using namespace Falcor;
class FakeInput : public RenderPass
{
private:
    FakeInput() {};

public:
    using SharedPtr = std::shared_ptr<FakeInput>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new FakeInput);}
    virtual std::string getDesc() override { return desc; }
    static const char* desc;

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void execute(RenderContext* context, const RenderData& data) override {}
    virtual void compile(RenderContext* context, const CompileData& data) override {}
};
