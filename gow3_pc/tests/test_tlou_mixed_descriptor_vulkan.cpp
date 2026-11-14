// SPDX-License-Identifier: GPL-2.0-or-later
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/renderer_vulkan/tlou_mixed_descriptor_template.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
static void OK(VkResult r) {
    if (r != VK_SUCCESS) { std::fprintf(stderr, "VkResult=%d\n", r); std::abort(); }
}
static std::vector<std::uint32_t> Load(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    assert(file);
    const auto bytes = static_cast<std::size_t>(file.tellg());
    assert(bytes % 4 == 0);
    std::vector<std::uint32_t> data(bytes / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), bytes);
    assert(file);
    return data;
}
static std::uint32_t ChooseMemory(VkPhysicalDevice physical, std::uint32_t bits,
                                   VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(physical, &mp);
    for (std::uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & required) == required)
            return i;
    }
    std::abort();
}
int main(int argc, char** argv) {
    assert(argc == 2);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance instance{};
    OK(vkCreateInstance(&ici, nullptr, &instance));
    std::uint32_t n = 0;
    OK(vkEnumeratePhysicalDevices(instance, &n, nullptr));
    assert(n > 0);
    std::vector<VkPhysicalDevice> physicals(n);
    OK(vkEnumeratePhysicalDevices(instance, &n, physicals.data()));
    const auto physical = physicals[0];
    std::uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qs(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, qs.data());
    std::uint32_t family = 0;
    while (family < qn && !(qs[family].queueFlags & VK_QUEUE_COMPUTE_BIT)) ++family;
    assert(family < qn);
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    const char* extension = "VK_KHR_push_descriptor";
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &extension;
    VkDevice device{};
    OK(vkCreateDevice(physical, &dci, nullptr, &device));
    const auto push = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
        vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));
    const auto push_template = reinterpret_cast<PFN_vkCmdPushDescriptorSetWithTemplateKHR>(
        vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetWithTemplateKHR"));
    assert(push && push_template);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdPushDescriptorSetWithTemplateKHR = push_template;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = 1024u * 1024u;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkBuffer buffer{};
    OK(vkCreateBuffer(device, &bci, nullptr, &buffer));
    VkMemoryRequirements bmr{};
    vkGetBufferMemoryRequirements(device, buffer, &bmr);
    VkMemoryAllocateInfo bmi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bmi.allocationSize = bmr.size;
    bmi.memoryTypeIndex = ChooseMemory(physical, bmr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory buffer_memory{};
    OK(vkAllocateMemory(device, &bmi, nullptr, &buffer_memory));
    OK(vkBindBufferMemory(device, buffer, buffer_memory, 0));
    void* mapped{};
    OK(vkMapMemory(device, buffer_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
    auto* words = static_cast<std::uint32_t*>(mapped);
    for (std::size_t i = 0; i < 1024 * 1024 / 4; ++i) words[i] = i * 7u + 11u;

    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R32_UINT;
    image_info.extent = {1, 1, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImage image{};
    OK(vkCreateImage(device, &image_info, nullptr, &image));
    VkMemoryRequirements imr{};
    vkGetImageMemoryRequirements(device, image, &imr);
    VkMemoryAllocateInfo imi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    imi.allocationSize = imr.size;
    imi.memoryTypeIndex = ChooseMemory(physical, imr.memoryTypeBits, 0);
    VkDeviceMemory image_memory{};
    OK(vkAllocateMemory(device, &imi, nullptr, &image_memory));
    OK(vkBindImageMemory(device, image, image_memory, 0));
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R32_UINT;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view{};
    OK(vkCreateImageView(device, &vci, nullptr, &view));
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 0.0f;
    VkSampler sampler{};
    OK(vkCreateSampler(device, &sci, nullptr, &sampler));

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = family;
    VkCommandPool pool{};
    OK(vkCreateCommandPool(device, &pci, nullptr, &pool));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd{};
    OK(vkAllocateCommandBuffers(device, &cai, &cmd));
    VkQueue queue{};
    vkGetDeviceQueue(device, family, 0, &queue);
    const auto begin = [&] {
        OK(vkResetCommandPool(device, pool, 0));
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        OK(vkBeginCommandBuffer(cmd, &bi));
    };
    const auto submit = [&] {
        OK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        OK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
        OK(vkQueueWaitIdle(queue));
    };
    // Upload a real uint pixel (7), with explicit transfer->compute ordering.
    begin();
    VkImageMemoryBarrier to_copy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_copy.srcAccessMask = 0;
    to_copy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_copy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_copy.srcQueueFamilyIndex = to_copy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_copy.image = image;
    to_copy.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_copy);
    VkClearColorValue pixel{};
    pixel.uint32[0] = 7;
    VkImageSubresourceRange image_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &pixel, 1, &image_range);
    VkImageMemoryBarrier to_shader = to_copy;
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_shader);
    submit();

    constexpr unsigned NumBindings = 6;
    constexpr std::array<VkDescriptorType, NumBindings> types{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER};
    using Slot = Vulkan::Detail::MixedDescriptorSlot<VkDescriptorBufferInfo, VkDescriptorImageInfo>;
    std::array<VkDescriptorSetLayoutBinding, NumBindings> bindings{};
    std::array<VkDescriptorUpdateTemplateEntry, NumBindings> entries{};
    std::array<VkWriteDescriptorSet, NumBindings> writes{};
    std::array<VkDescriptorBufferInfo, 4> buffers{};
    std::array<VkDescriptorImageInfo, 2> images{};
    images[0] = {VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[1] = {sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    for (unsigned i = 0; i < NumBindings; ++i) {
        bindings[i] = {i, types[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        entries[i] = {i, 0, 1, types[i], i * sizeof(Slot), sizeof(Slot)};
        writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i].dstBinding = i;
        writes[i].descriptorType = types[i];
        writes[i].descriptorCount = 1;
        if (i < 4) writes[i].pBufferInfo = &buffers[i];
        else writes[i].pImageInfo = &images[i-4];
    }
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    lci.bindingCount = NumBindings;
    lci.pBindings = bindings.data();
    VkDescriptorSetLayout set{};
    OK(vkCreateDescriptorSetLayout(device, &lci, nullptr, &set));
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, 4};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &set;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &range;
    VkPipelineLayout layout{};
    OK(vkCreatePipelineLayout(device, &plci, nullptr, &layout));
    VkDescriptorUpdateTemplateCreateInfo tci{VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
    tci.descriptorUpdateEntryCount = NumBindings;
    tci.pDescriptorUpdateEntries = entries.data();
    tci.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
    tci.pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    tci.pipelineLayout = layout;
    VkDescriptorUpdateTemplate update{};
    OK(vkCreateDescriptorUpdateTemplate(device, &tci, nullptr, &update));
    const auto code = Load(argv[1]);
    VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smi.codeSize = code.size() * 4;
    smi.pCode = code.data();
    VkShaderModule shader{};
    OK(vkCreateShaderModule(device, &smi, nullptr, &shader));
    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.layout = layout;
    cpci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = shader;
    cpci.stage.pName = "main";
    VkPipeline pipeline{};
    OK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline));

    std::vector<Slot> slots;
    slots.reserve(NumBindings);
    const auto pack = [&] {
        return Vulkan::Detail::PackMixedDescriptors(
            std::span<const VkWriteDescriptorSet>{writes},
            std::span<const VkDescriptorType>{types},
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLER, slots);
    };
    const auto template_push = [&] {
        assert(pack());
        Vulkan::Detail::PushMixedDescriptorTemplate(
            vk::CommandBuffer{cmd}, vk::DescriptorUpdateTemplate{update},
            vk::PipelineLayout{layout}, std::span<const Slot>{slots});
    };
    for (unsigned mode = 0; mode < 2; ++mode) {
        begin();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        for (std::uint32_t k = 0; k < 64; ++k) {
            for (unsigned i = 0; i < 3; ++i)
                buffers[i] = {buffer, i * 65536u + (k % 3u) * 4096u, 8192};
            buffers[3] = {buffer, 4 * 65536u + mode * 16384u, 8192};
            if (mode) template_push();
            else push(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, NumBindings, writes.data());
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &k);
            vkCmdDispatch(cmd, 1, 1, 1);
        }
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
        submit();
        for (std::uint32_t k = 0; k < 64; ++k) {
            std::uint32_t expected = 7;
            for (unsigned i = 0; i < 3; ++i)
                expected += words[(i * 65536u + (k % 3u) * 4096u) / 4u + k];
            const auto actual = words[(4 * 65536u + mode * 16384u) / 4u + k];
            if (actual != expected) {
                std::fprintf(stderr, "pixel mismatch mode=%u k=%u actual=%u expected=%u\n",
                             mode, k, actual, expected);
                return 1;
            }
        }
    }
    std::printf("Mixed Vulkan real pixels: 64 changing offsets per mode, sampled image+sampler+buffers PASS\n");
    // Conservative timing: the old path does not pay its normal deferred
    // copy/relocation cost here; the new path DOES pay the full pack cost.
    constexpr unsigned Calls = 2048;
    std::array<std::vector<double>, 2> costs;
    for (unsigned round = 0; round < 24; ++round) for (unsigned pass = 0; pass < 2; ++pass) {
        const unsigned mode = pass ^ (round & 1);
        begin();
        auto start = std::chrono::steady_clock::now();
        for (unsigned k = 0; k < Calls; ++k) {
            buffers[0].offset = (k % 8u) * 256u;
            if (mode) template_push();
            else push(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, NumBindings, writes.data());
        }
        OK(vkEndCommandBuffer(cmd));
        const auto end = std::chrono::steady_clock::now();
        if (round > 2)
            costs[mode].push_back(std::chrono::duration<double, std::nano>(end-start).count() / Calls);
    }
    for (auto& x : costs) std::sort(x.begin(), x.end());
    const double old_cost = costs[0][costs[0].size() / 2];
    const double new_cost = costs[1][costs[1].size() / 2];
    std::printf("Mixed Vulkan 6 bindings: old-command=%.1f ns, packed-template=%.1f ns, ratio=%.3f (software Vulkan, no CPU snapshots in old)\n",
                old_cost, new_cost, new_cost / old_cost);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroyDescriptorUpdateTemplate(device, update, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyDescriptorSetLayout(device, set, nullptr);
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, image_memory, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    vkUnmapMemory(device, buffer_memory);
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, buffer_memory, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}
