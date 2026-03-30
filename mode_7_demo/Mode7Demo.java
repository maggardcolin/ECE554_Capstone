import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.awt.image.DataBufferInt;
import java.io.File;
import java.io.IOException;
import javax.imageio.ImageIO;

public class Mode7Demo extends JPanel implements ActionListener, KeyListener {
    private static final int WIDTH = 1280;
    private static final int HEIGHT = 720;
    private static final int HORIZON = HEIGHT / 3;
    private static final int TEX_SIZE = 1024;
    private static final int FP_SHIFT = 6;
    private static final int FP_ONE = 1 << FP_SHIFT; // 256
    private static final int FP_HALF = FP_ONE >> 1; // 128

    private final BufferedImage screenImage;
    private int[] screenPixels;
    private int[] texturePixels;
    private int[] texturePixels2;
    private int[] skyPixels;  // renamed for clarity (was sky)

    // Camera/Player state (fixed point where appropriate)
    private int camX = 50 << FP_SHIFT; // 50.0
    private int camY = 50 << FP_SHIFT; // 50.0
    private int camZ = 10 << FP_SHIFT; // 8.0   ← typo in comment fixed but value unchanged
    private int angle = 0; // in "fixed point radians" — we'll use lookup or approx
    private int fov = 320 << FP_SHIFT; // 320.0

    // Movement flags
    private boolean up = false, down = false, left = false, right = false;
    private boolean w = false, a = false, s = false, d = false;

    // Simple sin/cos table (0..255 representing 0..2π)
    private static final int[] SIN_TABLE = new int[256];
    private static final int[] COS_TABLE = new int[256];

    static {
        for (int i = 0; i < 256; i++) {
            double rad = (i / 256.0) * 2 * Math.PI;
            SIN_TABLE[i] = (int) Math.round(Math.sin(rad) * FP_ONE);
            COS_TABLE[i] = (int) Math.round(Math.cos(rad) * FP_ONE);
        }
    }

    public Mode7Demo() {
        JFrame frame = new JFrame("render");
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setSize(WIDTH, HEIGHT);
        frame.setResizable(false);
        frame.add(this);
        frame.addKeyListener(this);
        frame.setVisible(true);

        screenImage = new BufferedImage(WIDTH, HEIGHT, BufferedImage.TYPE_INT_RGB);
        screenPixels = ((DataBufferInt) screenImage.getRaster().getDataBuffer()).getData();

        texturePixels = new int[TEX_SIZE * TEX_SIZE];
        texturePixels2 = new int[TEX_SIZE * TEX_SIZE];
        generateTexture();

        Timer timer = new Timer(16, this);
        timer.start();
    }

    private void generateTexture() {
        try {
            BufferedImage img = ImageIO.read(new File("map3.png"));
            texturePixels = convertTo256x256Array(img);
            BufferedImage img3 = ImageIO.read(new File("map2.png"));
            texturePixels2 = convertTo256x256Array(img3);

            BufferedImage img2 = ImageIO.read(new File("sky.jpg"));
            skyPixels = convertTo256x256Array(img2);   // now using same 1024×1024 resized format
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static int[] convertTo256x256Array(BufferedImage image) {
        BufferedImage resized = new BufferedImage(TEX_SIZE, TEX_SIZE, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = resized.createGraphics();
        g.drawImage(image, 0, 0, TEX_SIZE, TEX_SIZE, null);
        g.dispose();

        int[] pixels = new int[TEX_SIZE * TEX_SIZE];
        resized.getRGB(0, 0, TEX_SIZE, TEX_SIZE, pixels, 0, TEX_SIZE);
        return pixels;
    }

    @Override
    public void actionPerformed(ActionEvent e) {
        update();
        render();
        repaint();
    }

    private void update() {
        // Rotation (angle wraps 0..255)
        if (left) angle = (angle + 2) & 255;
        if (right) angle = (angle - 2) & 255;

        // Movement
        int speed = 2 << FP_SHIFT; // 2.0 in fixed point
        int dir = (angle + 64) & 255; // +90 degrees ≈ +64 in 256-step circle
        int dx = mulFP(COS_TABLE[dir], speed);
        int dy = mulFP(SIN_TABLE[dir], speed);

        if (up || w) {
            camX += dx;
            camY += dy;
        }
        if (down || s) {
            camX -= dx;
            camY -= dy;
        }
    }

    public static int get_uv(int screen_x, int screen_y, int player_x, int player_y, int player_z, int angle, int fov) {
        int sinA = SIN_TABLE[angle & 255];
        int cosA = COS_TABLE[angle & 255];
        int p = (screen_y - HORIZON) << FP_SHIFT; // distance from horizon
        int z = mulFPDiv(fov, player_z, p); // z = (fov * camZ) / p
        
        int offsetX = (screen_x - (WIDTH >> 1)) << FP_SHIFT; // signed
        int floorX = mulFPDiv(player_z, offsetX, fov);
        int floorY = player_z;
        // Rotate
        int worldX = mulFP(floorX, cosA) - mulFP(floorY, sinA);
        int worldY = mulFP(floorX, sinA) + mulFP(floorY, cosA);
        // Translate
        int texX = worldX + player_x;
        int texY = worldY + player_y;
        // Wrap texture coordinates (positive modulo)
        int u = (texX >> FP_SHIFT) & (TEX_SIZE - 1);
        int v = (texY >> FP_SHIFT) & (TEX_SIZE - 1);
        return (u<<16&0xFFFF0000)|(v);
    }

    private void render() {
        int sinA = SIN_TABLE[angle & 255];
        int cosA = COS_TABLE[angle & 255];

        // ────────────────────────────────────────────────
        // Sky rendering (simple cylindrical / rotated panorama)
        // ────────────────────────────────────────────────
        for (int y = 0; y <= HORIZON; y++) {
            // Simple panoramic sky: horizontal wrap + vertical stretch/compress
            int v = (y * TEX_SIZE / (HORIZON + 1)) & (TEX_SIZE - 1);

            for (int x = 0; x < WIDTH; x++) {
                // Rotate sky horizontally according to view angle
                int uOffset = 3*-(angle << 2) & (TEX_SIZE - 1);           // ×4 gives smoother turning
                int u = (x + uOffset) & (TEX_SIZE - 1);

                screenPixels[y * WIDTH + x] = skyPixels[v * TEX_SIZE + u];
            }
        }

        // ────────────────────────────────────────────────
        // Floor rendering (unchanged except variable rename)
        // ────────────────────────────────────────────────
        for (int y = HORIZON + 1; y < HEIGHT; y++) {
            int p = (y - HORIZON) << FP_SHIFT; // distance from horizon

            int z = mulFPDiv(fov, camZ, p); // z = (fov * camZ) / p

            for (int x = 0; x < WIDTH; x++) {
                int offsetX = (x - (WIDTH >> 1)) << FP_SHIFT; // signed

                int floorX = mulFPDiv(z, offsetX, fov);
                int floorY = z;

                // Rotate
                int worldX = mulFP(floorX, cosA) - mulFP(floorY, sinA);
                int worldY = mulFP(floorX, sinA) + mulFP(floorY, cosA);

                // Translate
                int texX = worldX + camX;
                int texY = worldY + camY;

                // Wrap texture coordinates (positive modulo)
                int u = (texX >> FP_SHIFT) & (TEX_SIZE - 1);
                int v = (texY >> FP_SHIFT) & (TEX_SIZE - 1);

				if( ((texturePixels[v * TEX_SIZE + u])>>24) !=0 )
                	screenPixels[y * WIDTH + x] = texturePixels[v * TEX_SIZE + u];
				else
                	screenPixels[y * WIDTH + x] = 0xDEADBEEF;
            }
        }
        /*for (int y = HORIZON + 1; y < HEIGHT; y++) {
            int p = (y - HORIZON) << FP_SHIFT; // distance from horizon

            int z = mulFPDiv(fov, camZ-(5<<FP_SHIFT), p); // z = (fov * camZ) / p

            for (int x = 0; x < WIDTH; x++) {
                int offsetX = (x - (WIDTH >> 1)) << FP_SHIFT; // signed

                int floorX = mulFPDiv(z, offsetX, fov);
                int floorY = z;

                // Rotate
                int worldX = mulFP(floorX, cosA) - mulFP(floorY, sinA);
                int worldY = mulFP(floorX, sinA) + mulFP(floorY, cosA);

                // Translate
                int texX = worldX + camX;
                int texY = worldY + camY;

                // Wrap texture coordinates (positive modulo)
                int u = (texX >> FP_SHIFT) & (TEX_SIZE - 1);
                int v = (texY >> FP_SHIFT) & (TEX_SIZE - 1);

				if( ((texturePixels2[v * TEX_SIZE + u])>>24) !=0 ){
					screenPixels[(y) * WIDTH + x] = texturePixels2[v * TEX_SIZE + u];
				}/*else{
					if(y<HEIGHT-1)
					screenPixels[(y) * WIDTH + x] = screenPixels[(y-1) * WIDTH + x];
				}* /
            }
            for (int x = 0; x < WIDTH; x++) {
            }
        }*/
    }

    // Fixed-point multiply (24.8 × 24.8 → 48.16 → take middle 32 bits → back to 24.8)
    private static int mulFP(int a, int b) {
        return (int) (((long) a * b) >> FP_SHIFT);
    }

    // Fixed-point multiply + divide: (a * b) / c
    private static int mulFPDiv(int a, int b, int c) {
        return (int) (((long) a * b) / c);
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        g.drawImage(screenImage, 0, 0, null);
        g.setColor(Color.WHITE);
        g.drawString("WASD / Arrows to move & turn", 10, 20);
        g.drawString("X: " + (camX >> FP_SHIFT) + " Y: " + (camY >> FP_SHIFT), 10, 35);
    }

    @Override
    public void keyPressed(KeyEvent e) {
        switch (e.getKeyCode()) {
            case KeyEvent.VK_W: case KeyEvent.VK_UP:    up = w = true; break;
            case KeyEvent.VK_S: case KeyEvent.VK_DOWN:  down = s = true; break;
            case KeyEvent.VK_A: case KeyEvent.VK_LEFT:  left = a = true; break;
            case KeyEvent.VK_D: case KeyEvent.VK_RIGHT: right = d = true; break;
        }
    }

    @Override
    public void keyReleased(KeyEvent e) {
        switch (e.getKeyCode()) {
            case KeyEvent.VK_W: case KeyEvent.VK_UP:    up = w = false; break;
            case KeyEvent.VK_S: case KeyEvent.VK_DOWN:  down = s = false; break;
            case KeyEvent.VK_A: case KeyEvent.VK_LEFT:  left = a = false; break;
            case KeyEvent.VK_D: case KeyEvent.VK_RIGHT: right = d = false; break;
        }
    }

    @Override
    public void keyTyped(KeyEvent e) {}

    public static void main(String[] args) {
        new Mode7Demo();
    }
}

