"""Phased whiteboard slide rendering for Lernvideos (imported from media_pipeline at runtime)."""
from __future__ import annotations

import os
import tempfile
from io import BytesIO
from typing import List, Optional, Tuple

SlideWeighted = List[Tuple[str, float]]


def _render_slide_raster(
    sc: dict,
    i: int,
    *,
    style: str,
    width: int,
    height: int,
    footer_h: int,
    margin: int,
    palettes,
    font_title,
    font_body,
    font_footer,
    font_badge,
    bold_path: Optional[str],
    regular_path: Optional[str],
    use_stock_images: bool,
    wb_phase: Optional[str] = None,
):
    import media_pipeline as mp
    from PIL import Image, ImageDraw, ImageFont

    is_title = str(sc.get("slide_kind") or "") == "title"
    title = str(sc.get("title") or (f"Szene {i+1}" if not is_title else "Lernvideo"))[:120]
    body = str(sc.get("body") or sc.get("bullets") or "")[:720]
    chapter = str(sc.get("chapter") or "").strip()
    top_c, bot_c, accent = palettes[i % len(palettes)]
    scene_accent = (42, 118, 92) if style == "whiteboard" else accent
    ph = wb_phase if style == "whiteboard" else None
    ch = height - footer_h

    def line_h(line: str, font) -> int:
        if not font:
            return 26
        _tmp = ImageDraw.Draw(Image.new("RGB", (4, 4)))
        b = _tmp.textbbox((0, 0), line or "Ay", font=font)
        return max(24, b[3] - b[1] + 8)

    photo_bytes = None
    if use_stock_images and not is_title:
        q = str(sc.get("image_query") or "").strip()
        if q:
            photo_bytes = mp.pexels_fetch_image_bytes(q)

    if style == "whiteboard":
        wb_bg = (242, 239, 230)
        img = Image.new("RGB", (width, height), color=wb_bg)
        if photo_bytes:
            try:
                bg = Image.open(BytesIO(photo_bytes)).convert("RGB").resize((width, ch), Image.Resampling.LANCZOS)
                img.paste(bg, (0, 0))
                wash = Image.new("RGBA", (width, ch), (250, 246, 236, 198))
                blended = Image.alpha_composite(bg.convert("RGBA"), wash).convert("RGB")
                img.paste(blended, (0, 0))
            except Exception:
                pass
        draw = ImageDraw.Draw(img)
        wb_accent = (42, 118, 92)
        mp._draw_whiteboard_doodles(draw, width, ch, i, wb_accent)
        draw.rounded_rectangle(
            [28, 28, width - 28, ch - 28],
            radius=8,
            outline=(198, 192, 176),
            width=2,
        )
    else:
        img = Image.new("RGB", (width, height), color=bot_c)
        draw = ImageDraw.Draw(img)
        if style == "rich":
            mp._draw_gradient_background(draw, width, ch, top_c, bot_c)
            draw.rectangle([0, 0, 12, ch], fill=accent)
        else:
            draw.rectangle([0, 0, width, ch], fill=bot_c)

    text_top_limit = height - footer_h - 24
    text_left = margin + (22 if style == "rich" else (28 if style == "whiteboard" else 0))

    if photo_bytes and style != "whiteboard":
        try:
            bg = Image.open(BytesIO(photo_bytes)).convert("RGB")
            bg = bg.resize((width, ch), Image.Resampling.LANCZOS)
            img.paste(bg, (0, 0))
            overlay = Image.new("RGBA", (width, ch), (12, 14, 35, 200))
            img = img.convert("RGBA")
            sub = img.crop((0, 0, width, ch))
            sub = Image.alpha_composite(sub.convert("RGBA"), overlay).convert("RGB")
            img.paste(sub, (0, 0))
            img = img.convert("RGB")
            draw = ImageDraw.Draw(img)
            if style == "rich":
                draw.rectangle([0, 0, 10, ch], fill=accent)
        except Exception:
            draw = ImageDraw.Draw(img)

    title_fill = (245, 248, 255)
    body_fill = (210, 218, 235)
    if photo_bytes:
        title_fill = (255, 255, 255)
        body_fill = (230, 235, 245)
    if style == "whiteboard":
        title_fill = (28, 32, 36)
        body_fill = (52, 60, 68)

    if is_title:
        ft_big = fb_sub = font_title
        try:
            if bold_path:
                ft_big = ImageFont.truetype(bold_path, 54)
            if regular_path:
                fb_sub = ImageFont.truetype(regular_path, 28)
        except OSError:
            pass
        if style == "rich":
            inset = 48
            draw.rounded_rectangle(
                [inset, inset, width - inset, height - footer_h - inset],
                radius=20,
                outline=accent,
                width=4,
            )
            inner = inset + 28
            draw.rounded_rectangle(
                [inner, inner, width - inner, height - footer_h - inner],
                radius=14,
                fill=(14, 16, 32),
            )
        elif style == "whiteboard":
            inset = 40
            draw.rounded_rectangle(
                [inset, inset, width - inset, height - footer_h - inset],
                radius=18,
                fill=(250, 247, 240),
                outline=scene_accent,
                width=3,
            )
        if style == "whiteboard" and ph == "empty":
            mp._draw_footer_bar(draw, width, height, font_footer, scene_accent)
            return img
        max_w = width - 2 * margin - 80
        y = (height - footer_h) // 3
        for line in mp._wrap_text(draw, title, ft_big, max_w):
            bbox = draw.textbbox((0, 0), line, font=ft_big)
            lw = bbox[2] - bbox[0]
            draw.text(((width - lw) // 2, y), line, fill=title_fill, font=ft_big)
            y += line_h(line, ft_big)
        y += 28
        for line in mp._wrap_text(draw, body, fb_sub, max_w - 40):
            bbox = draw.textbbox((0, 0), line, font=fb_sub)
            lw = bbox[2] - bbox[0]
            draw.text(((width - lw) // 2, y), line, fill=body_fill, font=fb_sub)
            y += line_h(line, fb_sub)
            if y > text_top_limit - 40:
                break
    else:
        show_badge = (
            bool(chapter)
            and style in ("rich", "whiteboard")
            and not (style == "whiteboard" and ph == "empty")
        )
        if show_badge:
            mp._draw_chapter_badge(
                draw,
                width,
                chapter,
                font_badge,
                scene_accent if style == "whiteboard" else accent,
                margin,
            )
        card_margin = margin
        card_top = margin + (52 if show_badge else 8)
        card_w = width - 2 * card_margin - (22 if style == "rich" else 0)
        tw = card_w - 24
        title_lines = mp._wrap_text(draw, title, font_title, tw)
        body_lines = mp._wrap_text(draw, body, font_body, tw)
        body_lines = body_lines[:10]
        if style == "rich" and not photo_bytes:
            est_h = 20 + len(title_lines) * 52 + 20 + min(len(body_lines), 12) * 36
            card_h = min(max(120, est_h), text_top_limit - card_top + 20)
            draw.rounded_rectangle(
                [text_left - 16, card_top - 12, text_left + card_w, card_top + card_h],
                radius=16,
                fill=(28, 30, 52),
                outline=accent,
                width=2,
            )
        elif style == "whiteboard":
            est_h = 20 + len(title_lines) * 52 + 20 + min(len(body_lines), 12) * 36
            card_h = min(max(120, est_h), text_top_limit - card_top + 20)
            draw.rounded_rectangle(
                [text_left - 16, card_top - 12, text_left + card_w, card_top + card_h],
                radius=14,
                fill=(252, 249, 242),
                outline=scene_accent,
                width=2,
            )

        if style == "whiteboard" and ph == "empty":
            mp._draw_footer_bar(draw, width, height, font_footer, scene_accent)
            return img

        y = card_top + 8
        for line in title_lines:
            draw.text((text_left, y), line, fill=title_fill, font=font_title)
            y += line_h(line, font_title)
        y += 16

        if style == "whiteboard" and ph == "title_only":
            mp._draw_footer_bar(draw, width, height, font_footer, scene_accent)
            return img

        lines_to_draw = body_lines
        if style == "whiteboard" and ph in ("empty", "title_only"):
            lines_to_draw = []
        elif style == "whiteboard" and ph == "body_partial" and len(body_lines) > 1:
            half = max(1, (len(body_lines) + 1) // 2)
            lines_to_draw = body_lines[:half]
        elif style == "whiteboard" and ph and str(ph).startswith("body_line_"):
            try:
                k = int(str(ph).split("_")[-1])
                lines_to_draw = body_lines[: max(0, min(k, len(body_lines)))]
            except ValueError:
                lines_to_draw = body_lines

        for line in lines_to_draw:
            if style == "whiteboard":
                draw.ellipse(
                    [text_left - 16, y + 4, text_left - 4, y + 16],
                    outline=scene_accent,
                    width=2,
                )
            draw.text((text_left, y), line, fill=body_fill, font=font_body)
            y += line_h(line, font_body)
            if y > text_top_limit:
                break

    mp._draw_footer_bar(draw, width, height, font_footer, scene_accent)
    return img


def render_scene_slides_weighted(
    scenes: List[dict],
    width: int,
    height: int,
    visual_style: str = "clean",
    use_stock_images: bool = False,
) -> SlideWeighted:
    """Returns (png_path, duration_share) with shares summing to 1.0."""
    try:
        from PIL import ImageFont
    except ImportError as e:
        raise RuntimeError("Pillow (PIL) wird für Lernvideos benötigt.") from e

    import media_pipeline as mp

    style = (visual_style or "clean").strip().lower()
    if style not in ("clean", "rich", "whiteboard"):
        style = "clean"

    font_title, font_body = mp._load_slide_fonts()
    bold_path, regular_path = mp._resolve_ttf_paths()
    font_footer = font_badge = None
    try:
        if regular_path:
            font_footer = ImageFont.truetype(regular_path, 17)
            font_badge = ImageFont.truetype(regular_path, 18)
    except OSError:
        font_footer = font_badge = font_body

    tmp_root = tempfile.mkdtemp(prefix="blop_video_")
    palettes = [
        ((24, 32, 72), (12, 14, 38), (94, 156, 255)),
        ((40, 22, 62), (14, 18, 42), (236, 112, 180)),
        ((18, 52, 48), (10, 22, 28), (80, 220, 200)),
        ((62, 28, 24), (20, 14, 22), (255, 180, 100)),
    ]
    footer_h = 46
    margin = 56

    n = len(scenes)
    if n == 0:
        return []
    scene_share = 1.0 / n
    out: SlideWeighted = []
    counter = 0

    for i, sc in enumerate(scenes):
        is_title = str(sc.get("slide_kind") or "") == "title"
        if style == "whiteboard":
            if is_title:
                phases: List[Tuple[str, float]] = [("empty", 0.35), ("full", 0.65)]
            else:
                body = str(sc.get("body") or sc.get("bullets") or "")
                from PIL import Image, ImageDraw

                d = ImageDraw.Draw(Image.new("RGB", (1, 1)))
                tw = width - 2 * margin - 24
                tl = mp._wrap_text(d, str(sc.get("title") or ""), font_title, tw)
                bl = mp._wrap_text(d, body, font_body, tw)
                bl = bl[:10]
                L = len(bl)
                if L <= 1:
                    phases = [("empty", 0.14), ("title_only", 0.22), ("body_line_1", 0.64)]
                else:
                    w_intro = 0.08 + 0.12
                    w_rest = 1.0 - w_intro
                    w_each = w_rest / float(L)
                    phases = [("empty", 0.08), ("title_only", 0.12)]
                    for k in range(1, L + 1):
                        phases.append((f"body_line_{k}", w_each))
            for pname, pw in phases:
                img = _render_slide_raster(
                    sc,
                    i,
                    style=style,
                    width=width,
                    height=height,
                    footer_h=footer_h,
                    margin=margin,
                    palettes=palettes,
                    font_title=font_title,
                    font_body=font_body,
                    font_footer=font_footer,
                    font_badge=font_badge,
                    bold_path=bold_path,
                    regular_path=regular_path,
                    use_stock_images=use_stock_images,
                    wb_phase=pname,
                )
                path = os.path.join(tmp_root, f"slide_{counter:04d}.png")
                img.save(path)
                counter += 1
                out.append((path, scene_share * pw))
        else:
            img = _render_slide_raster(
                sc,
                i,
                style=style,
                width=width,
                height=height,
                footer_h=footer_h,
                margin=margin,
                palettes=palettes,
                font_title=font_title,
                font_body=font_body,
                font_footer=font_footer,
                font_badge=font_badge,
                bold_path=bold_path,
                regular_path=regular_path,
                use_stock_images=use_stock_images,
                wb_phase=None,
            )
            path = os.path.join(tmp_root, f"slide_{counter:04d}.png")
            img.save(path)
            counter += 1
            out.append((path, scene_share))

    if out:
        tot = sum(w for _, w in out[:-1])
        lp, _ = out[-1]
        out[-1] = (lp, max(0.01, 1.0 - tot))

    return out
