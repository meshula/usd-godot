# Third-party code and assets

## godot-usd (MIT)

Portions of usd-godot are harvested from **godot-usd** by Miguel de Icaza
(<https://github.com/migueldeicaza>), used under the MIT License (© 2026 Miguel de Icaza).

**Currently included:**
- `tests/corpus/samples/` — USD sample assets, copied verbatim.
- `tests/corpus/tests/*.gd` and `tests/corpus/tests/run_all.gd` — behavior-level regression scripts,
  copied verbatim as fidelity oracles.

**Planned (tracked in `USD_GODOT_INTEGRATION_PLAN.md`):** transplanted translator code
(`usd_mesh_builder`, `usd_materials`, `usd_usdz`, `usd_curves`) and the non-destructive
source-preserving save design. Each transplanted file will retain an MIT attribution header and be
listed here as it lands.

### MIT License (godot-usd)

```
Copyright (c) 2026 Miguel de Icaza (https://github.com/migueldeicaza)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
