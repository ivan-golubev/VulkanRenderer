module;
#include <cassert>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ShaderProgram;

import Application;
import ErrorHandling;

namespace
{
	VkShaderModule createShaderModule(VkDevice device, std::string const& shaderBlob)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderBlob.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderBlob.data());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule;
	}

	std::string readFile(std::string const& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error(std::format("failed to open file: {}", filename));
		}

		size_t const fileSize = static_cast<size_t>(file.tellg());
		std::string buffer{};
		buffer.resize(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
}

namespace gg
{
	ShaderProgram::ShaderProgram(std::string const& vertexShaderAbsPath, std::string const& fragmentShaderAbsPath)
		: vertexShaderBlob{ readFile(vertexShaderAbsPath) }
		, fragmentShaderBlob{ readFile(fragmentShaderAbsPath) }
	{
		BreakIfFalse(vertexShaderBlob.size() != 0);
		BreakIfFalse(fragmentShaderBlob.size() != 0);

		device = Application::Get()->GetRenderer()->GetDevice();
		vertexShader = createShaderModule(device, vertexShaderBlob);
		fragmentShader = createShaderModule(device, fragmentShaderBlob);

		vertexShaderBlob.clear();
		fragmentShaderBlob.clear();
	}

	ShaderProgram::~ShaderProgram()
	{		
		if (vertexShader)
			vkDestroyShaderModule(device, vertexShader, nullptr);
		if (fragmentShader)
			vkDestroyShaderModule(device, fragmentShader, nullptr);
	}

	ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
	{
		vertexShader = nullptr;
		fragmentShader = nullptr;
		std::exchange(vertexShader, other.vertexShader);
		std::exchange(fragmentShader, other.fragmentShader);
	}

	ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
	{
		if (this != &other)
		{
			vertexShader = nullptr;
			fragmentShader = nullptr;
			std::exchange(vertexShader, other.vertexShader);
			std::exchange(fragmentShader, other.fragmentShader);
		}
		return *this;
	}

	VkShaderModule ShaderProgram::GetVertexShader() { return vertexShader; }
	VkShaderModule ShaderProgram::GetFragmentShader() { return fragmentShader; }

} // namespace gg
