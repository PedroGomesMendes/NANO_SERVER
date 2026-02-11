import socket
import threading
import numpy as np
from PyQt6.QtWidgets import QApplication, QLabel
from PyQt6.QtGui import QImage, QPixmap
from PyQt6.QtCore import pyqtSignal, QObject

IMG_W = 328
IMG_H = 320
RAW12_BYTES_PER_LINE = 492    # 328 pixels → 164 pares → 164 * 3 = 492
UDP_PACKET_SIZE = RAW12_BYTES_PER_LINE + 2  # linha + índice


def unpack_raw12_to_uint16(raw_line: bytes, width: int) -> np.ndarray:
    pixels = np.zeros(width, dtype=np.uint16)

    j = 0
    for i in range(0, width, 2):
        b0 = raw_line[j]
        b1 = raw_line[j + 1]
        b2 = raw_line[j + 2]

        # Formato RAW12 típico (MIPI/NV12)
        p0 = (b1 & 0x0F) << 8 | b0
        p1 = b2 << 4 | ((b1 & 0xF0) >> 4)

        # Ajusta o alinhamento correto aqui:
        pixels[i] = p0 << 4       # << 2 costuma ser o correto para DVP/SPI
        pixels[i + 1] = p1 << 4

        j += 3

    return pixels




class ImageReceiver(QObject):
    new_frame = pyqtSignal(np.ndarray)

    def __init__(self, port=5000):
        super().__init__()
        self.port = port
        self.running = True
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        print(f"Servidor UDP escutando na porta {self.port}")

        img = np.zeros((IMG_H, IMG_W), dtype=np.uint16)

        while self.running:
            data, _ = sock.recvfrom(UDP_PACKET_SIZE)

            if len(data) != UDP_PACKET_SIZE:
                print(f"Tamanho inesperado: {len(data)}")
                continue

            raw_line = data[:-2]
            line_index = int.from_bytes(data[-2:], "little")

            if 0 <= line_index < IMG_H:
                img[line_index] = unpack_raw12_to_uint16(raw_line, IMG_W)

            if line_index ==319:
                    img.astype(np.uint16).tofile("frame.raw")

            # Envia frame parcial para interface (evita lag)
            self.new_frame.emit(img.copy())





def update_label(img: np.ndarray):
    """
    Atualiza o QLabel com o frame recebido
    """
    # Cada pixel ocupa 2 bytes → pitch é width * 2
    qimg = QImage(
        img.data,
        IMG_W,
        IMG_H,
        IMG_W * 2,
        QImage.Format.Format_Grayscale16
    )

    label.setPixmap(QPixmap.fromImage(qimg))



if __name__ == "__main__":
    import sys

    app = QApplication(sys.argv)

    label = QLabel()
    label.setWindowTitle("UDP Image Receiver")
    label.resize(IMG_W, IMG_H)
    label.show()

    receiver = ImageReceiver(port=5000)
    receiver.new_frame.connect(update_label)

    sys.exit(app.exec())
