"""Generate icon.ico for Bricks Breaker Hit (no external assets needed)."""
from PIL import Image, ImageDraw

def make(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    s = size

    # Rounded dark gradient background
    bg_outer = (12, 14, 34, 255)
    bg_inner = (40, 60, 130, 255)
    # Radial gradient fake (concentric rounded rects)
    for i in range(s // 2, 0, -2):
        t = i / (s / 2)
        r = int(bg_outer[0] * t + bg_inner[0] * (1 - t))
        g = int(bg_outer[1] * t + bg_inner[1] * (1 - t))
        b = int(bg_outer[2] * t + bg_inner[2] * (1 - t))
        d.rounded_rectangle((s // 2 - i, s // 2 - i, s // 2 + i, s // 2 + i),
                            radius=max(2, i // 4), fill=(r, g, b, 255))

    # Bricks grid (top portion)
    brick_w = s * 0.28
    brick_h = s * 0.16
    rows = [
        # row_y_ratio, [(col_x_ratio, color)]
        (0.18, [(0.10, (240, 90, 90)), (0.40, (255, 165, 30)), (0.70, (90, 200, 230))]),
        (0.36, [(0.25, (180, 110, 230)), (0.55, (90, 230, 130))]),
    ]
    for y_r, cells in rows:
        y = int(y_r * s)
        for x_r, col in cells:
            x = int(x_r * s)
            d.rounded_rectangle((x, y, int(x + brick_w), int(y + brick_h)),
                                radius=max(1, int(s * 0.025)),
                                fill=col + (255,),
                                outline=(0, 0, 0, 120),
                                width=max(1, s // 64))
            # top highlight
            hi_x1 = int(x + brick_w - 2)
            hi_y1 = int(y + max(brick_h * 0.45, 4))
            if hi_x1 > x + 3 and hi_y1 > y + 3:
                d.rounded_rectangle((x + 2, y + 2, hi_x1, hi_y1),
                                    radius=max(1, int(s * 0.02)),
                                    fill=(255, 255, 255, 60))

    # Big glowing ball at bottom-right
    cx = int(s * 0.66)
    cy = int(s * 0.72)
    r = int(s * 0.18)
    # outer glow
    for gi in range(4, 0, -1):
        a = 30 + gi * 20
        d.ellipse((cx - r - gi * 4, cy - r - gi * 4, cx + r + gi * 4, cy + r + gi * 4),
                  fill=(120, 200, 255, a // gi))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(245, 250, 255, 255))
    # glint
    d.ellipse((cx - r * 0.4, cy - r * 0.4, cx, cy), fill=(255, 255, 255, 200))

    # Trajectory dots
    for i in range(5):
        t = i / 4
        px = int(cx - r - 4 - i * s * 0.06)
        py = int(cy + r * 0.2 + i * s * 0.04)
        rr = max(1, int(s * 0.025 * (1 - t * 0.5)))
        d.ellipse((px - rr, py - rr, px + rr, py + rr),
                  fill=(120, 200, 255, int(220 * (1 - t))))

    # Launcher / paddle hint bottom-left
    lx = int(s * 0.18)
    ly = int(s * 0.84)
    lr = int(s * 0.07)
    d.ellipse((lx - lr - 4, ly - lr - 4, lx + lr + 4, ly + lr + 4),
              fill=(60, 200, 255, 90))
    d.ellipse((lx - lr, ly - lr, lx + lr, ly + lr), fill=(255, 255, 255, 230))
    d.ellipse((lx - lr * 0.6, ly - lr * 0.6, lx + lr * 0.6, ly + lr * 0.6),
              fill=(60, 200, 255, 255))

    return img

sizes = [16, 24, 32, 48, 64, 128, 256]
images = [make(z) for z in sizes]
# Largest as base; other sizes as separate frames so each is hand-tuned
images[-1].save("icon.ico", format="ICO",
                sizes=[(z, z) for z in sizes],
                append_images=images[:-1])
print("icon.ico written, sizes:", sizes)
