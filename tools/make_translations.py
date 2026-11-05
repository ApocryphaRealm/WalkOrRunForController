#!/usr/bin/env python3
"""Writes dist/Interface/Translations/MovementSpeedSlider_<language>.txt (UTF-16LE with BOM) from the table below, so the
eleven files cannot drift apart. Run it after adding a string: py -3 tools/make_translations.py"""
import pathlib

LANGS = ["english", "chinese", "czech", "french", "german", "italian", "japanese", "korean", "polish", "russian", "spanish"]

T = {
"MSP_Row": ["Movement Speed", "移动速度", "Rychlost pohybu", "Vitesse de déplacement", "Bewegungsgeschwindigkeit", "Velocità di movimento", "移動速度", "이동 속도", "Prędkość ruchu", "Скорость передвижения", "Velocidad de movimiento"],
"MSP_Motion": ["Default Motion", "默认移动方式", "Výchozí pohyb", "Déplacement par défaut", "Standardbewegung", "Movimento predefinito", "標準の移動", "기본 이동", "Domyślny ruch", "Движение по умолчанию", "Movimiento predeterminado"],
"MSP_Run": ["Run", "奔跑", "Běh", "Course", "Rennen", "Corsa", "走る", "달리기", "Bieg", "Бег", "Correr"],
"MSP_Walk": ["Walk", "行走", "Chůze", "Marche", "Gehen", "Camminata", "歩く", "걷기", "Chód", "Ходьба", "Caminar"],
}

root = pathlib.Path(__file__).resolve().parent.parent / "dist" / "Interface" / "Translations"
root.mkdir(parents=True, exist_ok=True)
for i, lang in enumerate(LANGS):
    lines = []
    for key, row in T.items():
        assert len(row) == 11, key
        lines.append(f"${key}\t{row[i]}")
    (root / f"MovementSpeedSlider_{lang}.txt").write_bytes(b"\xff\xfe" + ("\r\n".join(lines) + "\r\n").encode("utf-16-le"))
print("wrote", len(LANGS), "files,", len(T), "keys")
