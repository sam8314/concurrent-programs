import java.util.Random;
/*
Space Station Simulation: 
A) Ordinary vehicles consume fuel
B) Supply vehicles supply fuel and consume some fuel for their return trip */
// ===========================================================
// Monitor: The Space Station is a synchronized object that protects the shared state---> current nitrogen and quantumFluid levels AND current number of docked vehicles
// - at most V vehicles can be docked at once, nitrogen  <= N, quantum <= Q.
// consumer can only take fuels if the levels allow so, suppliers can only deposit if levels allow deposit.
// n
// ===========================================================
class SpaceStationMonitor {

    // --- State Variables ---
    public final int maxDockingPlaces;
    public final int maxNitrogen; //N
    public final int maxQuantum; //Q
//  shared state variables protected by monitor lock
    private int dockedVehicles=0; //init to 0
    private int nitrogen; // nitrogen stored
    private int quantumFluid; // quantum stored


    // --- Constructor ---
    public SpaceStationMonitor(int V, int N, int Q) {

        this.maxDockingPlaces = V;
        this.maxNitrogen = N;
        this.maxQuantum = Q;
        //starting full - supplier must wait for space to be freed befofre depositing
        this.nitrogen = N;
        this.quantumFluid = Q;

    }
    //checks if station can take fuel
    private boolean canTake(int nReq, int qReq){ //nReq & qReq >=0
        return nitrogen >= nReq && quantumFluid >=qReq;
    }
    //checks if fuel can be deposited at station
    private boolean canDeposit(int nAdd, int qAdd){ //nAdd & qAdd >=0
        return nitrogen +nAdd<= maxNitrogen && quantumFluid+qAdd<= maxQuantum;
    }
    /**
     * called by a vehicle to request docking and the required fuel
     * a positive amount means it wants to take fuel 
     * a negative amount means it wants to deposit fuel 
     * The calling thread blocks until all the below conditions are satisfied: 1)docking place available, 2) enough fuel for take requests, 3) enough storage for deposit requests
     * ONLY ONE THREAD AT THE TIME CAN EVAL AND UPDATE THE STATE. WAITING THREADS RELEASE MONITOR LOCK WHEN CALLING WAIT()
     */

    // --- Monitor Methods ---
    public synchronized void requestDockAndFuel(int nReq, int qReq, String vehicleName) throws InterruptedException{
        //ARRIVAL & CURRENT STATE
        System.out.printf("--> %s arrives. Needs: N=%d, Q=%d. Station: Docks=%d/%d, N=%d/%d, Q=%d/%d%n",
                vehicleName, nReq, qReq,
                dockedVehicles, maxDockingPlaces,
                nitrogen, maxNitrogen,
                quantumFluid, maxQuantum);
        //math conversion into non neg take and dep parts
        int takeN = Math.max(0, nReq);
        int takeQ = Math.max(0, qReq);

        int depN = Math.max(0, -nReq);
        int depQ = Math.max(0, -qReq);

        

        //vehicle waits while until dock and transaction completion happens safely 
        while (true) { // to avoid other threads consuming or altering resources before a recheck 
            boolean dockFree = dockedVehicles <maxDockingPlaces; //COND A) DOCK AVAILABLE? 
            boolean take = canTake(takeN, takeQ); //COND B) ENOUGH FUEL TO TAKE?
            boolean deposit = canDeposit(depN, depQ); //COND C) ENOUGH FREE SPACE TO DEPOSIT?
            /*proceeds if all conditions are met */
            if(dockFree && take && deposit) { 
                break;
            }
           
            //prints why we wait
            System.out.printf(" %s waits. Need dock=%s, take=%s, dep=%s. Station: Docks=%d/%d, N=%d/%d, Q=%d/%d%n", vehicleName, dockFree? "OK" : "NO",
                take ? "OK": "NO", deposit ? "OK": "NO", dockedVehicles, maxDockingPlaces, nitrogen, maxNitrogen, quantumFluid, maxQuantum);
                /*release monitor lock
                sleeps until station state changes
                calls notifyAll() . when woke, re check cond*/
                wait();
        }
        //if all the above cond met, we can perform the dock (ATOMIC UPDATE)
        dockedVehicles++;
        //take (taken from storage)
        nitrogen-=takeN;
        quantumFluid-=takeQ;
        //deposit (added to storage)
        nitrogen += depN;
        quantumFluid += depQ;
        //station state after 
        System.out.printf("station state after %s: Docks=%d/%d, N=%d/%d, Q=%d/%d%n",
                vehicleName,
                dockedVehicles, maxDockingPlaces,
                nitrogen, maxNitrogen,
                quantumFluid, maxQuantum);
        notifyAll(); //wakes up all waiting threads so they can re check conditions as they may have been updated 
    }

    public synchronized void leaveStation(String vehicleName) {
        dockedVehicles--;
        System.out.printf("<-- %s leaves. Station: Docks=%d/%d, N=%d/%d, Q=%d/%d%n", vehicleName, dockedVehicles, maxDockingPlaces, nitrogen, maxNitrogen, quantumFluid, maxQuantum);
        notifyAll(); //wake all waiting vehicles to re-check their conditions

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
    private final int nSupply; // negative means deposit by our convention? we'll pass negative in main OR pass positive and negate
    private final int qSupply; 
    private final int nReqForReturn; //amount to take for return trip
    private final int qReqForReturn;
    private final Random rand = new Random();

    public SupplyVehicle(String name, SpaceStationMonitor station, int rounds,
                         int nSupply, int qSupply, int nReqForReturn, int qReqForReturn) {
        super(name);
        this.station = station;
        this.rounds = rounds;
        this.nReqForReturn = nReqForReturn;
        this.qReqForReturn = qReqForReturn;
        this.nSupply = nSupply;
        this.qSupply = qSupply;
    }

    @Override
    public void run() {
        for (int i = 0; i < rounds; i++) {
            try {
                // travel to the station
                Thread.sleep(rand.nextInt(600));

                // 1. arrive and deposit fuel (as a supplier)
                station.requestDockAndFuel(nSupply, qSupply, this.getName()+ "#"+ (i+1)+ "(unload)");
                Thread.sleep(rand.nextInt(150)); // Time to unload
                station.leaveStation(this.getName() +"#"+ (i+1)+ " (unload)");

                // 2. as an ordinary vehicle, request fuel for the return trip
                station.requestDockAndFuel(nReqForReturn, qReqForReturn, this.getName() +"#"+ (i+1)+ " (return)");
                Thread.sleep(rand.nextInt(150));

                // 3. leave the station
                station.leaveStation(this.getName() +"#"+ (i+1)+ " (return)");

                //return trip and next arrival
                Thread.sleep(rand.nextInt(1000));

            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.out.println(this.getName() + " interrupted.");
                break;
            }
        }
        System.out.println(this.getName() + " finished its rounds!");
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