import java.util.Random;

// ===========================================================
// Monitor: The Space Station
// ===========================================================
class SpaceStationMonitor {
    private final int maxDockingPlaces;
    private final int maxNitrogen;
    private final int maxQuantum;
    private int dockedVehicles = 0;
    private int nitrogen = 0;
    private int quantumFluid = 0;

    // --- Constructor ---
    public SpaceStationMonitor(int V, int N, int Q) {
        this.maxDockingPlaces = V;
        this.maxNitrogen = N;
        this.maxQuantum = Q;
        this.nitrogen = N; //start full
        this.quantumFluid = Q;
        System.out.printf("Station initialized: Docks=%d, N=%d, Q=%d%n", V, N, Q);
    }

    /**
     * called by a vehicle to request docking and the required fuel
     * a positive amount means it wants to take fuel 
     * a negative amount means it wants to deposit fuel 
     * blocks the vehicle until its request can be satisfied
     */
    public synchronized void requestDockAndFuel(int nReq, int qReq, String vehicleName) {
        // Determine if the vehicle is a consumer (needs fuel) or supplier (brings fuel)
        boolean isConsumer = (nReq > 0 || qReq > 0);
        boolean isSupplier = (nReq < 0 || qReq < 0);

        System.out.printf("--> %s arrives. Needs: N=%d, Q=%d. Station: Docks=%d/%d, N=%d/%d, Q=%d/%d%n",
                vehicleName, nReq, qReq,
                dockedVehicles, maxDockingPlaces,
                nitrogen, maxNitrogen,
                quantumFluid, maxQuantum);

        //vehicle waits while its conditions are not met
        while (true) {
            boolean canProceed = true;

            //check for free docking place
            if (dockedVehicles >= maxDockingPlaces) {
                canProceed = false;
            }

            //check fuel conditions
            if (isConsumer) {
                if (nReq > 0 && nitrogen < nReq) {canProceed = false;}
                if (qReq > 0 && quantumFluid < qReq) {canProceed = false;}
            } else if (isSupplier) {
                if (nReq < 0 && (nitrogen-nReq)<maxNitrogen) {canProceed = false;}
                if (qReq < 0 && (quantumFluid-qReq)>maxQuantum){canProceed = false;}
            }

            if (canProceed) {break;}

            //if conditions not met wait for notification
            try {
                System.out.printf("<-- %s waits. conditions not met%n", vehicleName);
                wait();
                System.out.printf("--> %s wakes up. re-checking conditions%n", vehicleName);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.out.printf("%s was interrupted%n", vehicleName);
                return;
            }
        }

        //dock and update fuel
        dockedVehicles++;
        if (isConsumer) {
            nitrogen -= nReq;
            quantumFluid -= qReq;
            System.out.printf("    %s fueled! it took: N=%d, Q=%d%n", vehicleName, nReq, qReq);
        } else if (isSupplier) {
            nitrogen += (-nReq); //make negative amount positive for addition
            quantumFluid += (-qReq);
            System.out.printf("    %s supplied fuel! it left: N=%d, Q=%d%n", vehicleName, (-nReq), (-qReq));
        }

        System.out.printf("    station after %s: Docks=%d/%d, N=%d/%d, Q=%d/%d%n",
                vehicleName,
                dockedVehicles, maxDockingPlaces,
                nitrogen, maxNitrogen,
                quantumFluid, maxQuantum);
    }

    /**
     * called by a vehicle when it leaves the station
     * frees up a docking place and notifies other waiting vehicles
     */
    public synchronized void leaveStation(String vehicleName) {
        dockedVehicles--;
        System.out.printf("<-- %s leaves. docks now: %d/%d. notifying others%n",
                vehicleName, dockedVehicles, maxDockingPlaces);
        notifyAll(); // wake up all waiting vehicles to re-check their conditions
    }
}

// ===========================================================
// Vehicle Threads
// ===========================================================
class OrdinaryVehicle extends Thread {
    private final SpaceStationMonitor station;
    private final int rounds;
    private final int nReq;
    private final int qReq;
    private final Random rand = new Random();

    public OrdinaryVehicle(String name, SpaceStationMonitor station, int rounds, int nReq, int qReq) {
        super(name);
        this.station = station;
        this.rounds = rounds;
        this.nReq = nReq;
        this.qReq = qReq;
    }

    @Override
    public void run() {
        for (int i = 0; i < rounds; i++) {
            try {
                //travel time to the station
                Thread.sleep(rand.nextInt(500));
                station.requestDockAndFuel(nReq, qReq, this.getName());

                //time spent fueling at the dock
                Thread.sleep(rand.nextInt(200));
                station.leaveStation(this.getName());

                //time until next arrival
                Thread.sleep(rand.nextInt(800));
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.out.println(this.getName() + " interrupted.");
                break;
            }
        }
        System.out.println(this.getName() + " finished its rounds.");
    }
}

class SupplyVehicle extends Thread {
    private final SpaceStationMonitor station;
    private final int rounds;
    private final int nSupply; 
    private final int qSupply; 
    private final int nReqForReturn; //amount to take for return trip
    private final int qReqForReturn;
    private final Random rand = new Random();

    public SupplyVehicle(String name, SpaceStationMonitor station, int rounds,
                         int nSupply, int qSupply, int nReqForReturn, int qReqForReturn) {
        super(name);
        this.station = station;
        this.rounds = rounds;
        this.nSupply = -Math.abs(nSupply); // make sure its neg
        this.qSupply = -Math.abs(qSupply);
        this.nReqForReturn = nReqForReturn;
        this.qReqForReturn = qReqForReturn;
    }

    @Override
    public void run() {
        for (int i = 0; i < rounds; i++) {
            try {
                // travel to the station
                Thread.sleep(rand.nextInt(600));

                // 1. arrive and deposit fuel (as a supplier)
                station.requestDockAndFuel(nSupply, qSupply, this.getName());
                Thread.sleep(rand.nextInt(150)); // Time to unload

                // 2. as an ordinary vehicle, request fuel for the return trip
                station.requestDockAndFuel(nReqForReturn, qReqForReturn, this.getName() + " (return)");
                Thread.sleep(rand.nextInt(150));

                // 3. leave the station
                station.leaveStation(this.getName() + " (combined)");

                //return trip and next arrival
                Thread.sleep(rand.nextInt(1000));

            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.out.println(this.getName() + " interrupted.");
                break;
            }
        }
        System.out.println(this.getName() + " finished its rounds.");
    }
}

public class SpaceStationSimulation {
    public static void main(String[] args) {
        SpaceStationMonitor station = new SpaceStationMonitor(2, 1000, 500);

        // create vehicles
        OrdinaryVehicle ship1 = new OrdinaryVehicle("Freighter-1", station, 5, 200, 0);
        OrdinaryVehicle ship2 = new OrdinaryVehicle("Freighter-2", station, 5, 0, 150);
        OrdinaryVehicle ship3 = new OrdinaryVehicle("Tanker-A", station, 4, 300, 100);
        SupplyVehicle supply1 = new SupplyVehicle("Supply-Drone-N", station, 3, -500, 0, 50, 0);
        SupplyVehicle supply2 = new SupplyVehicle("Supply-Drone-Q", station, 3, 0, -300, 0, 40);

        // start all threads
        ship1.start();
        ship2.start();
        ship3.start();
        supply1.start();
        supply2.start();

        // wait for all to finish
        try {
            ship1.join();
            ship2.join();
            ship3.join();
            supply1.join();
            supply2.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            System.out.println("[DEBUG] program interrupted");
        }

        System.out.println("[DEBUG] program finished");
    }
}
