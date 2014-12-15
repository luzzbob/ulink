package com.uniview.x.ulink;


import android.os.Handler;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Arrays;

/**
 * Sender class, using multi-cast packets to transform small data like: wifi ssid & password
 *
 */
public class Sender {
    /**
     * send thread started
     */
    public static final int MSG_START = 0;

    /**
     * send thread stopped
     */
    public static final int MSG_STOP = 1;

    private sendThread send_;

    /**
     * constructor.
     */
    public Sender() {
        send_ = new sendThread();
    }

    /**
     * set handler for handle START and STOP events.
     *
     * @param h the handler to receive the events.
     */
    public void setHandler(Handler h) {
        send_.setHandler(h);
    }

    /**
     * check current sender thread is running.
     *
     * @return true if running, else false.
     */
    public boolean isRunning() {
        return send_.isRunning();
    }

    /**
     * send bytes using ulink.
     * <p>please call the method when device is connected with wifi.</p>
     * If length is less then 1 or too long (>254), exception will throw.
     * Call this method will leads a new thread to sending packets, unless you call stop() method,
     * or error occurs.
     *
     * @param bytes  the bytes to sent.
     * @param offset where to sent.
     * @param count  sent bytes' length
     * @throws Exception
     */
    public void send(byte[] bytes, int offset, int count) throws Exception {
        send_.setData(Arrays.copyOfRange(bytes, offset, offset + count));
        send_.start();
    }

    /**
     * send bytes using ulink.
     *
     * @param bytes the bytes to sent.
     * @throws Exception
     * @see public void send(byte[] bytes, int offset, int count) throws Exception
     */
    public void send(byte[] bytes) throws Exception {
        send(bytes, 0, bytes.length);
    }

    /**
     * stop current sending thread.
     */
    public void stop() {
        send_.doStop();
    }

    /**
     * internal thread.
     */
    protected class sendThread extends Thread {
        DatagramSocket sock_ = null;
        private Handler handler_ = null;
        private boolean running_ = false;
        private byte[] data_ = null;
        private byte dataCrc_ = 0;

        /**
         * check current sender thread is running.
         * @return true if running, else false.
         */
        public boolean isRunning() {
            return running_;
        }

        /**
         * send bytes internal
         *
         * @param bytes bytes to send
         * @throws Exception
         */
        public void setData(byte[] bytes) throws Exception {
            if (bytes.length > 254) {
                throw new Exception("Message too long, support max 254 bytes.");
            }

            data_ = bytes;
            for (byte x : data_) {
                dataCrc_ ^= x;
            }
        }

        /**
         * set handler
         * @param h the handler to receive the events.
         */
        public void setHandler(Handler h) {
            handler_ = h;
        }

        /**
         * stop self.
         */
        public void doStop() {
            running_ = false;
        }

        @Override
        public void run() {
            running_ = true;

            if (handler_ != null) {
                handler_.obtainMessage(MSG_START).sendToTarget();
            }

            if (data_.length == 0) {
                running_ = false;
            }

            byte[] x = new byte[]{'x'};
            while (running_) {
                try {
                    Thread.sleep(10, 0);
                    if (data_ != null) {
                        if (sock_ == null) {
                            sock_ = new DatagramSocket();
                        }

                        // send first packet.
                        String sip = String.format("239.0.%d.%d", data_.length, dataCrc_);
                        InetAddress dst = InetAddress.getByName(sip);
                        DatagramPacket p = new DatagramPacket(x, 1, dst, 1200);
                        sock_.send(p);
                        Thread.sleep(3, 0);

                        for (int i = 0; i < data_.length; i += 2) {
                            if (i + 1 < data_.length) {
                                sip = String.format("239.%d.%d.%d", (i / 2) + 1, data_[i], data_[i + 1]);
                            } else {
                                sip = String.format("239.%d.%d.0", (i / 2) + 1, data_[i]);
                            }
                            dst = InetAddress.getByName(sip);
                            p.setAddress(dst);
                            sock_.send(p);
                        }
                    }
                } catch (Exception ex) {
                    ex.printStackTrace();
                }
            }

            if (sock_ != null) {
                sock_.close();
            }

            if (handler_ != null) {
                handler_.obtainMessage(MSG_STOP).sendToTarget();
            }
        }
    }


}
