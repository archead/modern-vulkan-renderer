# TODO

## Now — PBR
- [ ] Cook-Torrance BRDF in CalcPointLight
- [ ] Occlusion + emissive maps

## Next
- [ ] RAII for AllocatedBuffer / AllocatedImage → Model move = default
- [ ] Data-driven model loading (argv, then scene JSON)
- [ ] Binding collision: globalUbo / gBufferFragPos both at (0,0)

## Backlog
- [ ] KHR_texture_basisu + transcode
- [ ] .glb support
- [ ] Per-primitive AABB for frustum culling
- [ ] Sort draws by material
- [ ] Re-weld after tangent generation

## Known bugs
- [ ] baseColorFactor never populated in loader
- [ ] ObjectUBO::flags dead in deferred path

## Done
- [x] Pack metallic/roughness into G-buffer free alphas
- [x] attenK reads light[0] for every light
- [x] Split shaders in separate files
- [x] Decide: material factors in push constants vs per-material UBO
- [x] Per-primitive rendering
- [x] Node hierarchy traversal
- [x] KTX2 texture pipeline + cook script