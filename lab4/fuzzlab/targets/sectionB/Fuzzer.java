
import java.io.*;
import java.util.ArrayList;

public class Fuzzer {
    public static void getInput(String s, String o, String t) throws Exception {
        File normal = new File(s);
        FileInputStream inputStream = new FileInputStream(normal);
        if (o.length() > 0) {
            File outdir = new File(o);
            if (!outdir.exists())
                outdir.mkdir();
        }
        byte[] src = new byte[inputStream.available()];
        inputStream.read(src);
        File tryCrush = new File("crashinput");
        Runtime r = Runtime.getRuntime();
        tryCrush.createNewFile();
        //OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(tryCrush));
        int times = 0;
        int crashIndex = 1;
        while (true) {
            boolean flag = true;
            OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(tryCrush));
            if (times == 0) {
                outputStream.write(src);
            } else {
                for (int i = 0; i < src.length; i++) {
                    ArrayList<Byte> srcList = new ArrayList<>();
                    for (byte b : src)
                        srcList.add(b);
                    randomAdd(srcList);
                    randomAddSpace(srcList);
                    randomDecrease(srcList);
                    randomInsert(srcList);
                    randomNeg(srcList);
                    randomInsertMax(srcList);
                    int ranFreq = 1;
                    //OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(tryCrush));
                    for (int j = 0; j < ranFreq; j++) {
                        int ranStart = (int) (Math.random() * src.length);
                        byte[] temp = new byte[ranStart + (int) (Math.random() * src.length)];
                        for (int k = 0; k < temp.length; k++)
                            temp[k] = (byte) (Math.random() * Byte.MAX_VALUE);
                        outputStream.write(temp);
                        outputStream.write(src, ranStart, src.length - ranStart);
                        outputStream.flush();
                    }
                }
                Process p = r.exec(t + " crashinput");
                InputStream is = p.getInputStream();
                InputStreamReader ir = new InputStreamReader(is);
                BufferedReader br = new BufferedReader(ir);
                String str = br.readLine();
                if(str == null){
                    flag = false;
                    System.out.println("crash");
                    File success = new File(o + File.separator + "successcrash" + crashIndex++);
                    success.createNewFile();
                    InputStream inputStream1 = new BufferedInputStream(new FileInputStream(tryCrush));
                    byte[] temp = new byte[inputStream1.available()];
                    inputStream1.read(temp);
                    OutputStream outputStream1 = new BufferedOutputStream(new FileOutputStream(success));
                    outputStream1.write(temp);
                    outputStream1.close();
                }
                while (str != null) {
                    if (str.contains("fault") || str.contains("Error")) {
                        flag = false;
                        System.out.println("crash:" + str);
                        File success = new File(o + File.separator + "successcrashin" + crashIndex++);
                        success.createNewFile();
                        InputStream inputStream1 = new BufferedInputStream(new FileInputStream(tryCrush));
                        byte[] temp = new byte[inputStream1.available()];
                        inputStream1.read(temp);
                        OutputStream outputStream1 = new BufferedOutputStream(new FileOutputStream(success));
                        outputStream1.write(temp);
                        outputStream1.close();
                    } else {
                        System.out.println("no crash and the output is " + (str.length() > 10 ? str.substring(0, 10) : str)
                                + "......");
                    }
                    str = br.readLine();
                }
                tryCrush.delete();
                tryCrush.createNewFile();
                outputStream.close();
            }
            times++;
        }
    }

    private static void randomInsert(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            srcList.add(randomInsertPos, (byte) (Math.random() * Byte.MAX_VALUE));
        }
    }

    private static void randomAddSpace(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            srcList.add(randomInsertPos, (byte) 0);
        }
    }

    private static void randomDecrease(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            byte last = srcList.get(randomInsertPos);
            last--;
            srcList.add(randomInsertPos, last);
        }
    }

    private static void randomAdd(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            byte last = srcList.get(randomInsertPos);
            last++;
            srcList.add(randomInsertPos, last);
        }
    }

    private static void randomNeg(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            byte last = srcList.get(randomInsertPos);
            last = (byte) ~last;
            srcList.add(randomInsertPos, last);
        }
    }

    private static void randomInsertMax(ArrayList<Byte> srcList) {
        int ranFreq = (int) (Math.random() * 1000);
        for (int j = 0; j < ranFreq; j++) {
            int randomInsertPos = (int) (srcList.size() * Math.random());
            srcList.add(randomInsertPos, Byte.MAX_VALUE);
        }
    }

    public static void main(String[] args) {
        try {
            BufferedReader bf = new BufferedReader(new InputStreamReader(System.in));
            System.out.println("input path:");
            String s = bf.readLine();
            System.out.println("output path:");
            String o = bf.readLine();
            System.out.println("target:");
            String t = bf.readLine();
            getInput(s, o, t);
        } catch (Exception e) {
            e.printStackTrace();
        }

    }
}
