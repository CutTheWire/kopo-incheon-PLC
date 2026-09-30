from PIL import Image, ImageDraw, ImageFont
import os

def draw_download_icon(draw, center_x, center_y, size, color):
    """다운로드 아이콘(화살표 + 트레이)을 직접 그리는 함수"""
    stroke = int(size * 0.12)
    top_y = center_y - size * 0.38
    arrow_bottom_y = center_y + size * 0.10
    
    # 1. 화살표 세로선
    draw.line([(center_x, top_y), (center_x, arrow_bottom_y)], fill=color, width=stroke)
    
    # 2. 화살표 머리 (V자)
    head_w = size * 0.30
    head_h = size * 0.22
    draw.line([
        (center_x - head_w, arrow_bottom_y - head_h),
        (center_x, arrow_bottom_y),
        (center_x + head_w, arrow_bottom_y - head_h)
    ], fill=color, width=stroke, joint="miter")
    
    # 3. 받침대 트레이 (U자)
    tray_w = size * 0.42
    tray_y = center_y + size * 0.35
    tray_h = size * 0.16
    draw.line([
        (center_x - tray_w, tray_y - tray_h),
        (center_x - tray_w, tray_y),
        (center_x + tray_w, tray_y),
        (center_x + tray_w, tray_y - tray_h)
    ], fill=color, width=stroke, joint="miter")

def create_download_banner(output_filename="plc_download_banner.png", bg_mode="transparent"):
    # 타겟 이미지 크기 (700x140)
    target_w, target_h = 700, 140
    scale = 4  # 고화질 Anti-aliasing
    w, h = target_w * scale, target_h * scale
    radius = 20 * scale

    # 배경 투명도 설정
    if bg_mode == "transparent":
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    else:
        img = Image.new("RGBA", (w, h), (255, 255, 255, 255))

    draw = ImageDraw.Draw(img)

    # 초록색 둥근 사각형 배경
    green_color = (27, 122, 62, 255)
    draw.rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=green_color)

    # 폰트 로드
    font_paths = [
        "C:/Windows/Fonts/malgunbd.ttf",
        "C:/Windows/Fonts/malgun.ttf",
        "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf",
        "/System/Library/Fonts/AppleSDGothicNeo.ttc"
    ]
    
    font_title = None
    for fp in font_paths:
        if os.path.exists(fp):
            try:
                font_title = ImageFont.truetype(fp, 28 * scale)
                break
            except Exception:
                pass
    
    if font_title is None:
        font_title = ImageFont.load_default(size=28 * scale)

    title_text = "PLC, HIM 실행파일 Download"

    # 텍스트 크기 산출
    title_bbox = draw.textbbox((0, 0), title_text, font=font_title)
    title_w = title_bbox[2] - title_bbox[0]
    title_h = title_bbox[3] - title_bbox[1]

    # 아이콘 및 레이아웃 (수평/수직 중앙 정렬)
    icon_size = 32 * scale
    icon_gap = 16 * scale
    header_total_w = icon_size + icon_gap + title_w

    header_x = (w - header_total_w) / 2
    header_y = (h - title_h) / 2

    icon_center_x = header_x + icon_size / 2
    icon_center_y = header_y + title_h / 2

    # 다운로드 아이콘 및 제목 그리기
    draw_download_icon(draw, icon_center_x, icon_center_y, icon_size, (255, 255, 255, 255))
    
    title_x = header_x + icon_size + icon_gap
    draw.text((title_x, header_y), title_text, fill=(255, 255, 255, 255), font=font_title)

    # 최종 이미지 축소 (Anti-aliasing)
    img_final = img.resize((target_w, target_h), Image.Resampling.LANCZOS)
    img_final.save(output_filename)
    print(f"배너 생성 완료: {output_filename}")

if __name__ == "__main__":
    create_download_banner("IMG/plc_download_banner.png", bg_mode="transparent")