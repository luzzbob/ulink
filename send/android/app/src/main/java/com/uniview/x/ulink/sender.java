package com.uniview.x.ulink;


import android.os.Handler;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Arrays;

/**
 * Created by matrix on 2014/12/12.
 */
public class Sender {

    public static final int MSG_START = 0;
    public static final int MSG_STOP = 1;
    private sendThread send_;
    ;

    /*
     * @description:
     */
    public Sender() {
        send_ = new sendThread();
    }

    public void setHandler(Handler h) {
        send_.setHandler(h);
    }

    public boolean isRunning() {
        return send_.isRunning();
    }

    public void send(byte[] bytes, int offset, int count) {
        send_.setData(Arrays.copyOfRange(bytes, offset, offset + count));
        send_.start();
    }

    public void send(byte[] bytes) {
        send(bytes, 0, bytes.length);
    }

    public void stop() {
        send_.doStop();
    }

    protected class sendThread extends Thread {
        DatagramSocket sock_ = null;
        private Handler handler_ = null;
        private boolean running_ = false;
        private byte[] data_ = null;

        public boolean isRunning() {
            return running_;
        }

        public void setData(byte[] bytes) {
            data_ = bytes;
        }

        public void setHandler(Handler h) {
            handler_ = h;
        }

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

            byte[] x = new byte[] {'x'};
            while (running_) {
                try {
                    Thread.sleep(10, 0);
                    if (data_ != null) {
                        if (sock_ == null) {
                            sock_ = new DatagramSocket();
                        }

                        // send first packet.
                        String sip = String.format("239.0.%d.%d", data_.length, data_[0]);
                        InetAddress dst = InetAddress.getByName(sip);
                        DatagramPacket p = new DatagramPacket(x, 1, dst, 1200);
                        sock_.send(p);
                        Thread.sleep(1,0);

                        for (int i = 1; i < data_.length; i += 2) {
                            if (i + 1 < data_.length) {
                                sip = String.format("239.%d.%d.%d", (i + 1) / 2, data_[i], data_[i + 1]);
                            } else {
                                sip = String.format("239.%d.%d.0", (i + 1) / 2, data_[i]);
                            }
                            dst = InetAddress.getByName(sip);
                            p.setAddress(dst);
                            sock_.send(p);
                            Thread.sleep(1,0);
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
