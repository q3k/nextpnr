/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  Clifford Wolf <clifford@symbioticeda.com>
 *  Copyright (C) 2018  David Shah <david@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "nextpnr.h"
#include "placer1.h"
#include "router1.h"

NEXTPNR_NAMESPACE_BEGIN

static std::tuple<int, int, std::string> split_identifier_name(const std::string &name)
{
    size_t first_slash = name.find('/');
    NPNR_ASSERT(first_slash != std::string::npos);
    size_t second_slash = name.find('/', first_slash + 1);
    NPNR_ASSERT(second_slash != std::string::npos);
    return std::make_tuple(std::stoi(name.substr(1, first_slash)),
                           std::stoi(name.substr(first_slash + 2, second_slash - first_slash)),
                           name.substr(second_slash + 1));
};

// -----------------------------------------------------------------------

void IdString::initialize_arch(const BaseCtx *ctx)
{
    // Nothing here -- IdString is actually initialized in the constructor,
    // because we need to have bba loaded.
}

// -----------------------------------------------------------------------

BelId Arch::getBelByName(IdString name) const
{
    BelId ret;
    auto it = bel_by_name.find(name);
    if (it != bel_by_name.end())
        return it->second;

    Location loc;
    std::string basename;
    std::tie(loc.x, loc.y, basename) = split_identifier_name(name.str(this));
    ret.location = loc;
    IdString basename_id = id(basename);
    auto &tt = getTileType(loc.x, loc.y);
    for (int i = 0; i < tt.num_bels; i++) {
        if (tt.bels[i].name_id == basename_id.index) {
            ret.index = i;
            bel_by_name[name] = ret;
            return ret;
        }
    }
    return BelId();
}

WireId Arch::getBelPinWire(BelId bel, IdString pin) const
{
    WireId ret;

    NPNR_ASSERT(bel != BelId());

    int num_bel_wires = getBelTypeInfo(bel).num_pins;
    const BelTypePinPOD *bel_type_wires = getBelTypeInfo(bel).pins.get();
    for (int i = 0; i < num_bel_wires; i++)
        if (bel_type_wires[i].name_id == pin.index) {
            ret.location = bel.location;
            ret.index = getTileTypeBel(bel).pin_wires[i];
            break;
        }

    return ret;
}

PortType Arch::getBelPinType(BelId bel, IdString pin) const
{
    NPNR_ASSERT(bel != BelId());

    int num_bel_wires = getBelTypeInfo(bel).num_pins;
    const BelTypePinPOD *bel_type_wires = getBelTypeInfo(bel).pins.get();
    for (int i = 0; i < num_bel_wires; i++)
        if (bel_type_wires[i].name_id == pin.index) {
            bool is_in = bel_type_wires[i].flags & BelTypePinPOD::FLAG_INPUT;
            bool is_out = bel_type_wires[i].flags & BelTypePinPOD::FLAG_OUTPUT;
            if (is_in && is_out)
                return PORT_INOUT;
            if (is_in)
                return PORT_IN;
            assert(is_out);
            return PORT_OUT;
        }

    return PORT_INOUT;
}

// -----------------------------------------------------------------------

WireId Arch::getWireByName(IdString name) const
{
    WireId ret;
    auto it = wire_by_name.find(name);
    if (it != wire_by_name.end())
        return it->second;

    Location loc;
    std::string basename;
    std::tie(loc.x, loc.y, basename) = split_identifier_name(name.str(this));
    ret.location = loc;
    IdString basename_id = id(basename);
    auto &tt = getTileType(loc.x, loc.y);
    for (int i = 0; i < tt.num_wires; i++) {
        if (tt.wires[i].name_id == basename_id.index) {
            ret.index = i;
            wire_by_name[name] = ret;
            return ret;
        }
    }
    return WireId();
}

// -----------------------------------------------------------------------

PipId Arch::getPipByName(IdString name) const
{
    auto it = pip_by_name.find(name);
    if (it != pip_by_name.end())
        return it->second;

    PipId ret;
    Location loc;
    std::string basename;
    std::tie(loc.x, loc.y, basename) = split_identifier_name(name.str(this));
    ret.location = loc;
    AllPipRange range;
    range.b.cursor_tile = loc.x + device_info->width * loc.y;
    range.b.cursor_kind = PIP_KIND_PIP;
    range.b.cursor_index = 0;
    range.b.cursor_subindex = -1;
    range.b.device = device_info;
    range.b.family = family_info;
    ++range.b;
    range.e.cursor_tile = loc.x + device_info->width * loc.y + 1;
    range.e.cursor_kind = PIP_KIND_PIP;
    range.e.cursor_index = 0;
    range.e.cursor_subindex = -1;
    range.e.device = device_info;
    range.e.family = family_info;
    ++range.e;
    for (const auto& curr: range) {
        pip_by_name[getPipName(curr)] = curr;
    }
    if (pip_by_name.find(name) == pip_by_name.end())
        NPNR_ASSERT_FALSE_STR("no pip named " + name.str(this));
    return pip_by_name[name];
}

IdString Arch::getPipName(PipId pip) const
{
    NPNR_ASSERT(pip != PipId());

    int x = pip.location.x;
    int y = pip.location.y;

    if (pip.kind == PIP_KIND_PIP) {
      std::string src_name = getWireBasename(getPipSrcWire(pip)).str(this);
      std::string dst_name = getWireBasename(getPipDstWire(pip)).str(this);
      return id("X" + std::to_string(x) + "/Y" + std::to_string(y) + "/" + src_name + ".->." + dst_name);
    } else {
      auto &tt = getTileType(pip.location);
      std::string port_name = IdString(tt.ports[pip.index].name_id).str(this);
      std::string dst_name = getWireBasename(getPipDstWire(pip)).str(this);
      return id("X" + std::to_string(x) + "/Y" + std::to_string(y) + "/" + port_name + "/" + std::to_string(pip.subindex) + ".->." + dst_name);
    }
}

// -----------------------------------------------------------------------

void PipIterator::operator++() {
    cursor_index++;
    auto &ttw = arch->getTileTypeWire(wire);
    if (stage == STAGE_PIPS) {
        int num;
        if (mode == MODE_UPHILL)
            num = ttw.num_pip_dst_xrefs;
        else
            num = ttw.num_pip_src_xrefs;
        if (cursor_index == num) {
            cursor_index = 0;
            stage = STAGE_PORTS;
        }
    }
    if (stage == STAGE_PORTS) {
        while (true) {
            if (cursor_index == ttw.num_port_xrefs) {
                cursor_index = 0;
                stage = STAGE_END;
                break;
            }
            // Make sure the port is connected.
            auto &tile = arch->getTile(wire.location);
            auto &xref = ttw.port_xrefs[cursor_index];
            if (tile.conns[xref.port_idx].port_idx != -1)
                break;
            cursor_index++;
        }
    }
}

PipId PipIterator::operator*() const {
    PipId ret;
    auto &ttw = arch->getTileTypeWire(wire);
    ret.location = wire.location;
    if (mode == MODE_UPHILL) {
        if (stage == STAGE_PIPS) {
            ret.kind = PIP_KIND_PIP;
            ret.index = ttw.pip_dst_xrefs[cursor_index];
        } else {
            ret.kind = PIP_KIND_PORT;
            auto &xref = ttw.port_xrefs[cursor_index];
            ret.index = xref.port_idx;
            ret.subindex = xref.wire_idx;
        }
    } else {
        if (stage == STAGE_PIPS) {
            ret.kind = PIP_KIND_PIP;
            ret.index = ttw.pip_src_xrefs[cursor_index];
        } else {
            ret.kind = PIP_KIND_PORT;
            auto &tile = arch->getTile(wire.location);
            auto &xref = ttw.port_xrefs[cursor_index];
            auto &conn = tile.conns[xref.port_idx];
            ret.location.x = conn.tile_x;
            ret.location.y = conn.tile_y;
            ret.index = conn.port_idx;
            ret.subindex = xref.wire_idx;
        }
    }
    return ret;
}

// -----------------------------------------------------------------------
//
// XXX package pins

std::vector<IdString> Arch::getBelPins(BelId bel) const
{
    std::vector<IdString> ret;
    NPNR_ASSERT(bel != BelId());

    int num_bel_wires = getBelTypeInfo(bel).num_pins;
    const BelTypePinPOD *bel_type_wires = getBelTypeInfo(bel).pins.get();

    for (int i = 0; i < num_bel_wires; i++) {
        IdString id;
        id.index = bel_type_wires[i].name_id;
        ret.push_back(id);
    }

    return ret;
}

// -----------------------------------------------------------------------

bool Arch::place() { return placer1(getCtx(), Placer1Cfg(getCtx())); }

bool Arch::route() { return router1(getCtx(), Router1Cfg(getCtx())); }

// -----------------------------------------------------------------------

std::vector<GraphicElement> Arch::getDecalGraphics(DecalId decal) const
{
    std::vector<GraphicElement> ret;

    if (decal.type == DecalId::TYPE_BEL) {
        BelId bel;
        bel.index = decal.z;
        bel.location = decal.location;
        // XXX
#if 0
        int z = locInfo(bel)->bel_data[bel.index].z;
        auto bel_type = getBelType(bel);

        if (bel_type == id_TRELLIS_SLICE) {
            GraphicElement el;
            el.type = GraphicElement::TYPE_BOX;
            el.style = decal.active ? GraphicElement::STYLE_ACTIVE : GraphicElement::STYLE_INACTIVE;
            el.x1 = bel.location.x + logic_cell_x1;
            el.x2 = bel.location.x + logic_cell_x2;
            el.y1 = bel.location.y + logic_cell_y1 + (z)*logic_cell_pitch;
            el.y2 = bel.location.y + logic_cell_y2 + (z)*logic_cell_pitch;
            ret.push_back(el);
        }

        if (bel_type == id_TRELLIS_IO) {
            GraphicElement el;
            el.type = GraphicElement::TYPE_BOX;
            el.style = decal.active ? GraphicElement::STYLE_ACTIVE : GraphicElement::STYLE_INACTIVE;
            el.x1 = bel.location.x + logic_cell_x1;
            el.x2 = bel.location.x + logic_cell_x2;
            el.y1 = bel.location.y + logic_cell_y1 + (2 * z) * logic_cell_pitch;
            el.y2 = bel.location.y + logic_cell_y2 + (2 * z + 0.5f) * logic_cell_pitch;
            ret.push_back(el);
        }
#endif
    }

    return ret;
}

DecalXY Arch::getBelDecal(BelId bel) const
{
    DecalXY decalxy;
    decalxy.decal.type = DecalId::TYPE_BEL;
    decalxy.decal.location = bel.location;
    decalxy.decal.z = bel.index;
    decalxy.decal.active = !checkBelAvail(bel);
    return decalxy;
}

DecalXY Arch::getWireDecal(WireId wire) const { return {}; }

DecalXY Arch::getPipDecal(PipId pip) const { return {}; };

DecalXY Arch::getGroupDecal(GroupId pip) const { return {}; };

NEXTPNR_NAMESPACE_END
