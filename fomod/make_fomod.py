#!/usr/bin/env python3
"""Generate the FOMOD: one WhereWasI.ini per timing choice plus fomod/ModuleConfig.xml.

FOMOD cannot write a value the user types, so each answer installs a prepared INI.
Run from anywhere: python3 fomod/make_fomod.py   (then package with release.py)
Template: ../WhereWasI.ini (its fMinHoursAway line is replaced per option).
"""
import re
import sys
import tomllib
from pathlib import Path
from xml.sax.saxutils import escape

MOD = Path(__file__).resolve().parent.parent
DAYS = [1, 2, 3, 4, 5, 6]
WEEKS = [1, 2, 3, 4]
DEFAULT = ("days", 1)
INI_DEST = "SKSE/Plugins/WhereWasI.ini"


def label(unit: str, n: int) -> str:
    return f"{n} {unit[:-1] if n == 1 else unit}"


def options():
    for unit, counts, hours in (("days", DAYS, 24), ("weeks", WEEKS, 168)):
        for n in counts:
            yield unit, n, n * hours, f"fomod/options/{n}{unit[0]}/WhereWasI.ini"  # archive path = source path


def write_inis() -> list[str]:
    template = (MOD / "WhereWasI.ini").read_text(encoding="utf-8")
    assert re.search(r"(?m)^fMinHoursAway=", template), "template has no fMinHoursAway line"
    paths = []
    for unit, n, hours, rel in options():
        text = re.sub(r"(?m)^fMinHoursAway=.*$",
                      f"; Chosen in the installer: {label(unit, n)}. 24 = 1 day, 168 = 1 week.\nfMinHoursAway={hours}",
                      template)
        out = MOD / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))
        paths.append(rel)
    return paths


def plugin(name: str, desc: str, body: str, kind: str) -> str:
    return (f'<plugin name="{escape(name)}"><description>{escape(desc)}</description>{body}'
            f'<typeDescriptor><type name="{kind}"/></typeDescriptor></plugin>')


def step(name: str, group: str, gtype: str, plugins: list[str], visible_unit: str | None = None) -> str:
    vis = (f'<visible><flagDependency flag="unit" value="{visible_unit}"/></visible>' if visible_unit else "")
    return (f'<installStep name="{escape(name)}">{vis}<optionalFileGroups order="Explicit">'
            f'<group name="{escape(group)}" type="{gtype}"><plugins order="Explicit">{"".join(plugins)}'
            f'</plugins></group></optionalFileGroups></installStep>')


def module_config() -> str:
    unit_step = step("Recap timing", "Show the recap after you have been away for at least", "SelectExactlyOne", [
        plugin(u.capitalize(), f"Pick a number of {u} on the next page.",
               f'<conditionFlags><flag name="unit">{u}</flag></conditionFlags>',
               "Recommended" if u == DEFAULT[0] else "Optional")
        for u in ("days", "weeks")])
    count_steps = []
    for unit in ("days", "weeks"):
        plugins = [plugin(label(unit, n),
                          f"Show the recap when the save is at least {label(unit, n)} old (fMinHoursAway={hours}).",
                          f'<files><file source="{rel}" destination="{INI_DEST}"/></files>',
                          "Recommended" if (unit, n) == DEFAULT else "Optional")
                   for u, n, hours, rel in options() if u == unit]
        count_steps.append(step(f"How many {unit}", f"How many {unit}?", "SelectExactlyOne", plugins, unit))
    info = step("Settings file", "Where your choice is saved", "SelectAll", [plugin(
        "WhereWasI.ini",
        "Your choice is written to Data/SKSE/Plugins/WhereWasI.ini (in MO2: this mod's folder, SKSE/Plugins/WhereWasI.ini). "
        "Edit it any time: fMinHoursAway is in hours (24 = 1 day, 168 = 1 week, 0 = every load). "
        "bShowQuest, bShowLocation and bShowSnapshot (level, gold, bounty) turn lines on (1) or off (0). "
        "Changes take effect the next time the game starts.",
        # The schema requires files or flags on every option; this page installs nothing.
        '<conditionFlags><flag name="infoShown">1</flag></conditionFlags>', "Required")])
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
            'xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">'
            '<moduleName>Where Was I</moduleName>'
            '<requiredInstallFiles><file source="SKSE/Plugins/WhereWasI.dll" destination="SKSE/Plugins/WhereWasI.dll"/>'
            '</requiredInstallFiles>'
            f'<installSteps order="Explicit">{unit_step}{"".join(count_steps)}{info}</installSteps></config>\n')


def info_xml(cfg: dict) -> str:
    site = f"https://github.com/{cfg['github']['repo']}" if cfg.get("github", {}).get("repo") else ""
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<fomod>'
            f"<Name>{escape(cfg['display_name'])}</Name><Author>Kautsar Bana</Author>"
            f"<Version>{escape(cfg['version'])}</Version><Description>{escape(cfg['description'])}</Description>"
            f"<Website>{escape(site)}</Website></fomod>\n")


def main() -> int:
    cfg = tomllib.loads((MOD / "release.toml").read_text(encoding="utf-8"))
    inis = write_inis()
    (MOD / "fomod" / "ModuleConfig.xml").write_text(module_config(), encoding="utf-8")
    (MOD / "fomod" / "info.xml").write_text(info_xml(cfg), encoding="utf-8")
    # The archive is built from release.toml, so every generated file must be listed there.
    missing = [p for p in inis + ["fomod/ModuleConfig.xml", "fomod/info.xml"] if cfg["files"].get(p) != p]
    if missing:
        print("release.toml [files] is missing:\n" + "\n".join(f'"{p}" = "{p}"' for p in missing))
        return 1
    print(f"wrote {len(inis)} INIs, ModuleConfig.xml, info.xml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
