package io.bvg.viewer;

import atlantafx.base.theme.PrimerDark;
import javafx.application.Application;
import javafx.embed.swing.SwingFXUtils;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.Separator;
import javafx.scene.control.Alert;
import javafx.scene.image.ImageView;
import javafx.scene.image.WritableImage;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;

import io.bvg.BvgFile;
import io.bvg.Generated;
import io.bvg.Node;
import io.bvg.Style;
import io.bvg.Gradient;

// Java2D (только нужное, без java.awt.*)
import java.awt.AlphaComposite;
import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.Paint;
import java.awt.RenderingHints;
import java.awt.TexturePaint;
import java.awt.geom.AffineTransform;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Line2D;
import java.awt.geom.Path2D;
import java.awt.geom.Point2D;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.awt.LinearGradientPaint;
import java.awt.RadialGradientPaint;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

public final class JfxViewer extends Application {
    private final List<int[]> palette = new ArrayList<>();
    private final List<Gradient> grads = new ArrayList<>();
    private final List<Style> styles = new ArrayList<>();
    private final List<Node> nodes = new ArrayList<>();
    private final List<BufferedImage> images = new ArrayList<>();

    private ImageView imageView;
    private double zoom = 1.0;
    private int canvasW = 1, canvasH = 1;
    private final Label zoomLbl = new Label("100%");

    private WritableImage cachedImage;

    @Override
    public void start(Stage stage) {
        Application.setUserAgentStylesheet(new PrimerDark().getUserAgentStylesheet());

        Button openBtn = new Button("Open .bvg");
        Button zoomOut = new Button("−");
        Button zoomIn = new Button("+");
        Button fit = new Button("Fit");
        Button reset = new Button("Reset");
        zoomLbl.getStyleClass().add("zoom-label");

        openBtn.setOnAction(e -> openFile(stage));
        zoomIn.setOnAction(e -> { zoom = Math.min(50, zoom * 1.25); updateZoomLabel(); redraw(); });
        zoomOut.setOnAction(e -> { zoom = Math.max(0.05, zoom / 1.25); updateZoomLabel(); redraw(); });
        fit.setOnAction(e -> { fitToWindow(); updateZoomLabel(); redraw(); });
        reset.setOnAction(e -> { zoom = 1.0; imageView.setTranslateX(0); imageView.setTranslateY(0); updateZoomLabel(); redraw(); });

        HBox toolbar = new HBox(8, openBtn, new Separator(), zoomOut, zoomIn, fit, reset, zoomLbl);
        toolbar.setAlignment(Pos.CENTER_LEFT);
        toolbar.setPadding(new Insets(8, 12, 8, 12));
        toolbar.getStyleClass().add("toolbar");

        imageView = new ImageView();
        imageView.setPreserveRatio(true);
        imageView.setSmooth(true);

        StackPane center = new StackPane(imageView);
        center.setStyle("-fx-background-color: #1e1e1e;");
        ScrollPane scroll = new ScrollPane(center);
        scroll.setFitToWidth(true);
        scroll.setFitToHeight(true);
        scroll.setPannable(true);
        scroll.getStyleClass().add("edge-to-edge");
        VBox.setVgrow(scroll, Priority.ALWAYS);

        imageView.setOnMousePressed(e -> { imageView.setUserData(new double[]{e.getX(), e.getY()}); });
        imageView.setOnMouseDragged(e -> {
            double[] start = (double[]) imageView.getUserData();
            if (start == null) return;
            imageView.setTranslateX(imageView.getTranslateX() + (e.getX() - start[0]) * zoom);
            imageView.setTranslateY(imageView.getTranslateY() + (e.getY() - start[1]) * zoom);
            imageView.setUserData(new double[]{e.getX(), e.getY()});
        });

        imageView.setOnScroll(e -> {
            double factor = e.getDeltaY() > 0 ? 1.1 : 0.9;
            zoom = Math.max(0.05, Math.min(50, zoom * factor));
            updateZoomLabel();
            redraw();
        });

        Label statusFile = new Label("-");
        Label statusSize = new Label("-");
        Label statusZoom = new Label("-");
        HBox status = new HBox(24, statusFile, statusSize, statusZoom);
        status.setPadding(new Insets(4, 12, 4, 12));
        status.getStyleClass().add("status-bar");

        stage.setUserData(new Object[]{statusFile, statusSize, statusZoom});

        BorderPane root = new BorderPane();
        root.setTop(toolbar);
        root.setCenter(scroll);
        root.setBottom(status);

        Scene scene = new Scene(root, 1200, 800);
        scene.getStylesheets().add(
            JfxViewer.class.getResource("/bvg-viewer.css").toExternalForm()
        );
        stage.setScene(scene);
        stage.setTitle("BVG Viewer");
        stage.show();
    }

    private void updateZoomLabel() {
        zoomLbl.setText(String.format("%d%%", (int) (zoom * 100)));
    }

    private void openFile(Stage stage) {
        FileChooser fc = new FileChooser();
        fc.getExtensionFilters().add(new FileChooser.ExtensionFilter("BVG", "*.bvg"));
        File f = fc.showOpenDialog(stage);
        if (f == null) return;
        try {
            BvgFile bf = BvgFile.read(f.toPath());
            palette.clear(); grads.clear(); styles.clear();
            nodes.clear(); images.clear();
            byte[] raw = bf.getChunk("COLR");
            if (raw != null) palette.addAll(Generated.unpackColr(raw));
            raw = bf.getChunk("GRAD");
            if (raw != null) grads.addAll(Generated.unpackGrad(raw));
            raw = bf.getChunk("STYL");
            if (raw != null) styles.addAll(Generated.unpackStyl(raw));
            raw = bf.getChunk("NODE");
            if (raw != null) nodes.addAll(Generated.unpackNode(raw));
            raw = bf.getChunk("IMGS");
            if (raw != null) {
                for (var im : Generated.unpackImages(raw)) {
                    if (im.format == Generated.IMG_RAW_RGBA) {
                        BufferedImage img = new BufferedImage(im.width, im.height, BufferedImage.TYPE_INT_ARGB);
                        for (int y = 0; y < im.height; y++) {
                            for (int x = 0; x < im.width; x++) {
                                int i = (y * im.width + x) * 4;
                                int r = im.data[i] & 0xFF;
                                int g = im.data[i + 1] & 0xFF;
                                int b = im.data[i + 2] & 0xFF;
                                int a = im.data[i + 3] & 0xFF;
                                img.setRGB(x, y, (a << 24) | (r << 16) | (g << 8) | b);
                            }
                        }
                        images.add(img);
                    } else {
                        images.add(javax.imageio.ImageIO.read(new ByteArrayInputStream(im.data)));
                    }
                }
            }
            canvasW = bf.width;
            canvasH = bf.height;

            renderToCache();
            fitToWindow();
            redraw();

            Object[] s = (Object[]) stage.getUserData();
            ((Label) s[0]).setText(f.getName());
            ((Label) s[1]).setText(bf.width + "×" + bf.height);
        } catch (Exception ex) {
            new Alert(Alert.AlertType.ERROR, ex.getMessage()).showAndWait();
        }
    }

    private void fitToWindow() {
        if (canvasW <= 0 || canvasH <= 0 || imageView.getScene() == null) return;
        double w = Math.max(100, imageView.getScene().getWidth() - 40);
        double h = Math.max(100, imageView.getScene().getHeight() - 120);
        double sx = w / canvasW;
        double sy = h / canvasH;
        zoom = Math.min(sx, sy) * 0.95;
        imageView.setTranslateX(0);
        imageView.setTranslateY(0);
    }

    private void redraw() {
        if (cachedImage == null) return;
        imageView.setImage(cachedImage);
        imageView.setScaleX(zoom);
        imageView.setScaleY(zoom);
    }

    // ============================================================
    //  JAVA2D RENDER
    // ============================================================

    private void renderToCache() {
        BufferedImage img = new BufferedImage(canvasW, canvasH, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);

        g.setComposite(AlphaComposite.Clear);
        g.fillRect(0, 0, canvasW, canvasH);
        g.setComposite(AlphaComposite.SrcOver);

        drawRange(g, 0, nodes.size());
        g.dispose();

        cachedImage = SwingFXUtils.toFXImage(img, null);
    }

    private Color colorAt(int idx) {
        if (idx == 0xFFFF || idx >= palette.size()) return new Color(0, 0, 0, 0);
        int[] c = palette.get(idx);
        return new Color(c[0], c[1], c[2], c[3]);
    }

    private Paint brushFor(Style s) {
        if (s == null || s.fillKind == 0) return null;
        if (s.fillKind == 1) return colorAt(s.fillIdx);
        if (s.fillIdx >= grads.size()) return null;
        Gradient g = grads.get(s.fillIdx);

        if (g.kind == Gradient.PATTERN) {
            int img_idx = (int) g.geometry[0];
            if (img_idx < images.size() && images.get(img_idx) != null) {
                BufferedImage img = images.get(img_idx);
                // Правильный TexturePaint: только BufferedImage + Rectangle2D
                return new TexturePaint(img, new Rectangle2D.Double(0, 0, img.getWidth(), img.getHeight()));
            }
            return null;
        }

        float[] fractions = new float[g.stops.size()];
        Color[] colors = new Color[g.stops.size()];
        for (int i = 0; i < g.stops.size(); i++) {
            Gradient.Stop st = g.stops.get(i);
            fractions[i] = st.offset;
            colors[i] = (st.colorIdx == 0xFFFF)
                ? new Color(st.rgba[0], st.rgba[1], st.rgba[2], st.rgba[3])
                : colorAt(st.colorIdx);
        }

        if (g.kind == Gradient.LINEAR) {
            return new LinearGradientPaint(
                (float) g.geometry[0], (float) g.geometry[1],
                (float) g.geometry[2], (float) g.geometry[3],
                fractions, colors);
        } else if (g.kind == Gradient.RADIAL) {
            return new RadialGradientPaint(
                new Point2D.Float((float) g.geometry[0], (float) g.geometry[1]),
                (float) g.geometry[2],
                fractions, colors);
        }
        return null;
    }

    private BasicStroke strokeFor(Style s) {
        if (s == null || s.strokeKind == 0) return null;
        int cap = s.cap == 0 ? BasicStroke.CAP_BUTT :
                  s.cap == 1 ? BasicStroke.CAP_ROUND : BasicStroke.CAP_SQUARE;
        int join = s.join == 0 ? BasicStroke.JOIN_MITER :
                   s.join == 1 ? BasicStroke.JOIN_ROUND : BasicStroke.JOIN_BEVEL;
        float[] dash = null;
        if (!s.dash.isEmpty()) {
            dash = new float[s.dash.size()];
            for (int i = 0; i < dash.length; i++) dash[i] = s.dash.get(i).floatValue();
        }
        return new BasicStroke(s.strokeWidth, cap, join, s.miterLimit, dash, 0);
    }

    private Path2D buildPath(Node.PathCmd[] cmds) {
        Path2D.Double path = new Path2D.Double();
        double cur_x = 0, cur_y = 0;
        for (Node.PathCmd c : cmds) {
            switch (c.op) {
                case 0 -> { path.moveTo(c.args[0], c.args[1]); cur_x = c.args[0]; cur_y = c.args[1]; }
                case 1 -> { path.lineTo(c.args[0], c.args[1]); cur_x = c.args[0]; cur_y = c.args[1]; }
                case 2 -> { path.quadTo(c.args[0], c.args[1], c.args[2], c.args[3]); cur_x = c.args[2]; cur_y = c.args[3]; }
                case 3 -> { path.curveTo(c.args[0], c.args[1], c.args[2], c.args[3], c.args[4], c.args[5]); cur_x = c.args[4]; cur_y = c.args[5]; }
                case 4 -> { svgArc(path, cur_x, cur_y, c.args[0], c.args[1], c.args[2], c.args[3], c.args[4], c.args[5], c.args[6]); cur_x = c.args[5]; cur_y = c.args[6]; }
                case 5 -> path.closePath();
            }
        }
        return path;
    }

    private static void svgArc(Path2D path, double x1, double y1, double rx, double ry, double rot, double large_arc, double sweep, double x2, double y2) {
        if (rx == 0 || ry == 0) { path.lineTo(x2, y2); return; }
        rx = Math.abs(rx); ry = Math.abs(ry);
        double cos_phi = Math.cos(rot), sin_phi = Math.sin(rot);
        double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2;
        double x1p =  cos_phi * dx + sin_phi * dy;
        double y1p = -sin_phi * dx + cos_phi * dy;
        double lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
        if (lam > 1) { double s = Math.sqrt(lam); rx *= s; ry *= s; }
        double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        if (den == 0) { path.lineTo(x2, y2); return; }
        double coef = Math.sqrt(Math.max(0, num / den));
        if (large_arc == sweep) coef = -coef;
        double cxp =  coef * (rx * y1p) / ry;
        double cyp = -coef * (ry * x1p) / rx;
        double cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2;
        double cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2;
        double theta1 = Math.atan2((y1p - cyp) / ry, (x1p - cxp) / rx);
        double theta2 = Math.atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx);
        double dtheta = theta2 - theta1;
        if (sweep == 0 && dtheta > 0) dtheta -= 2 * Math.PI;
        if (sweep != 0 && dtheta < 0) dtheta += 2 * Math.PI;
        int n = Math.max(16, (int) Math.ceil(Math.abs(dtheta) * 64));
        path.lineTo(x1, y1);
        for (int k = 1; k <= n; k++) {
            double t = theta1 + dtheta * k / n;
            double ex = rx * Math.cos(t);
            double ey = ry * Math.sin(t);
            double px = cos_phi * ex - sin_phi * ey + cx;
            double py = sin_phi * ex + cos_phi * ey + cy;
            if (k == n) path.lineTo(x2, y2);
            else path.lineTo(px, py);
        }
    }

    private void drawRange(Graphics2D g, int start, int end) {
        int i = start;
        while (i < end) {
            Node nd = nodes.get(i);
            if (nd.type == Node.DEF) {
                i += 1 + nd.childCount;
                continue;
            }
            Style st = (nd.style < styles.size()) ? styles.get(nd.style) : null;

            Graphics2D g2 = (Graphics2D) g.create();
            if (nd.transform != null) {
                g2.transform(new AffineTransform(
                    nd.transform[0], nd.transform[1], nd.transform[2],
                    nd.transform[3], nd.transform[4], nd.transform[5]));
            }
            if (st != null && st.opacity < 255) {
                g2.setComposite(AlphaComposite.getInstance(AlphaComposite.SRC_OVER, st.opacity / 255f));
            }

            switch (nd.type) {
                case Node.GROUP -> {
                    int cc = nd.childCount;
                    drawRange(g2, i + 1, i + 1 + cc);
                    i += 1 + cc;
                    g2.dispose();
                    continue;
                }
                case Node.RECT -> {
                    if (st != null && st.fillKind != 0) {
                        g2.setPaint(brushFor(st));
                        g2.fill(new Rectangle2D.Double(nd.rect[0], nd.rect[1], nd.rect[2], nd.rect[3]));
                    }
                    if (st != null && st.strokeKind != 0) {
                        g2.setPaint(st.strokeKind == 2 ? brushFor(st) : colorAt(st.strokeIdx));
                        g2.setStroke(strokeFor(st));
                        g2.draw(new Rectangle2D.Double(nd.rect[0], nd.rect[1], nd.rect[2], nd.rect[3]));
                    }
                }
                case Node.ELLIPSE -> {
                    double x = nd.ellipse[0] - nd.ellipse[2];
                    double y = nd.ellipse[1] - nd.ellipse[3];
                    double w = nd.ellipse[2] * 2;
                    double h = nd.ellipse[3] * 2;
                    if (st != null && st.fillKind != 0) {
                        g2.setPaint(brushFor(st));
                        g2.fill(new Ellipse2D.Double(x, y, w, h));
                    }
                    if (st != null && st.strokeKind != 0) {
                        g2.setPaint(st.strokeKind == 2 ? brushFor(st) : colorAt(st.strokeIdx));
                        g2.setStroke(strokeFor(st));
                        g2.draw(new Ellipse2D.Double(x, y, w, h));
                    }
                }
                case Node.LINE -> {
                    if (st != null && st.strokeKind != 0) {
                        g2.setPaint(st.strokeKind == 2 ? brushFor(st) : colorAt(st.strokeIdx));
                        g2.setStroke(strokeFor(st));
                        g2.draw(new Line2D.Double(nd.line[0], nd.line[1], nd.line[2], nd.line[3]));
                    }
                }
                case Node.POLYLINE, Node.POLYGON -> {
                    if (nd.points == null || nd.points.length == 0) break;
                    Path2D p = new Path2D.Double();
                    for (int k = 0; k < nd.points.length; k++) {
                        double px = nd.points[k][0], py = nd.points[k][1];
                        if (k == 0) p.moveTo(px, py); else p.lineTo(px, py);
                    }
                    if (nd.type == Node.POLYGON) p.closePath();
                    if (st != null && st.fillKind != 0) {
                        g2.setPaint(brushFor(st));
                        g2.fill(p);
                    }
                    if (st != null && st.strokeKind != 0) {
                        g2.setPaint(st.strokeKind == 2 ? brushFor(st) : colorAt(st.strokeIdx));
                        g2.setStroke(strokeFor(st));
                        g2.draw(p);
                    }
                }
                case Node.PATH -> {
                    Path2D p = buildPath(nd.path);
                    if (st != null && st.fillKind != 0) {
                        g2.setPaint(brushFor(st));
                        g2.fill(p);
                    }
                    if (st != null && st.strokeKind != 0) {
                        g2.setPaint(st.strokeKind == 2 ? brushFor(st) : colorAt(st.strokeIdx));
                        g2.setStroke(strokeFor(st));
                        g2.draw(p);
                    }
                }
                case Node.TEXT -> {
                    g2.setFont(new Font("SansSerif", Font.PLAIN, 12));
                    if (st != null) {
                        g2.setPaint(colorAt(st.fillIdx));
                        g2.drawString(nd.text, (float) nd.tx, (float) nd.ty);
                    }
                }
                case Node.IMAGE -> {
                    if (nd.imageIdx < images.size() && images.get(nd.imageIdx) != null) {
                        g2.drawImage(images.get(nd.imageIdx), (int) nd.rect[0], (int) nd.rect[1], (int) nd.rect[2], (int) nd.rect[3], null);
                    }
                }
                case Node.USE -> {
                    int def_idx = nd.defIdx;
                    int di = 0;
                    int def_start = -1;
                    for (int j = 0; j < nodes.size(); j++) {
                        if (nodes.get(j).type == Node.DEF) {
                            if (di == def_idx) { def_start = j; break; }
                            di++;
                        }
                    }
                    if (def_start >= 0) {
                        int cc = nodes.get(def_start).childCount;
                        g2.translate(nd.tx, nd.ty);
                        drawRange(g2, def_start + 1, def_start + 1 + cc);
                    }
                }
            }
            g2.dispose();
            i++;
        }
    }

    public static void main(String[] args) {
        launch(args);
    }
}