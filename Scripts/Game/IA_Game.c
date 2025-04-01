
///////////////////////////////////////////////////////////////////////
// 9) IA_Game SINGLETON
///////////////////////////////////////////////////////////////////////
class IA_Game
{
	static ref array<ref IA_Area> s_allAreas = {};
    static private ref IA_Game m_instance = null;
    static bool HasInstance()
    {
        return (m_instance != null);
    }

    static ref RandomGenerator rng = new RandomGenerator();

    private bool m_periodicTaskActive = false;
    private ref array<IA_AreaInstance> m_areas = {};
	static private bool beenInstantiated = false;
    static private ref array<IEntity> m_entityGc = {};

    private bool m_hasInit = false;

    void Init()
    {
        //Print("[DEBUG] IA_Game.Init called.", LogLevel.NORMAL);
        if (m_hasInit)
            return;

        m_hasInit = true;
        ActivatePeriodicTask();
    }
	IA_AreaInstance getActiveArea(){
		return m_areas[0];
	}
	static IA_Game Instantiate()
	{
		if(!Replication.IsServer()){
			return m_instance;
		}
		//Print("[DEBUG] IA_Game.Instantiate called.", LogLevel.NORMAL);
		
	    if (beenInstantiated){
			//Print("[DEBUG] IA_Game.Instantiate Already Instantiated. = "+beenInstantiated, LogLevel.NORMAL);
	        return m_instance;
		}
		//Print("[DEBUG] IA_Game.Instantiate Going.", LogLevel.NORMAL);
		
	    m_instance = new IA_Game();
	    m_instance.Init();
		beenInstantiated = true;
	    return m_instance;
	}
	
/*
    void ~IA_Game()
    {
        m_areas.Clear();
        m_entityGc.Clear();
    }
*/
    static void AddEntityToGc(IEntity e)
    {
        m_entityGc.Insert(e);
    }

    private static void EntityGcTask()
    {
        if (m_entityGc.IsEmpty())
            return;
        delete m_entityGc[0];
        m_entityGc.Remove(0);
    }
	
	private void ActivatePeriodicTask()
	{
	    if (m_periodicTaskActive)
	        return;
	    m_periodicTaskActive = true;
	    // Use a static wrapper so the instance’s PeriodicalGameTask is called correctly.
	    GetGame().GetCallqueue().CallLater(PeriodicalGameTaskWrapper, 200, true);
	    GetGame().GetCallqueue().CallLater(EntityGcTask, 250, true);
	}

	// Static wrapper that retrieves the singleton instance and calls its PeriodicalGameTask.
	static void PeriodicalGameTaskWrapper()
	{
	    IA_Game gameInstance = IA_Game.Instantiate();
	    if (gameInstance)
	    {
	        gameInstance.PeriodicalGameTask();
	    }
	}


	void PeriodicalGameTask()
	{
	    //Print("IA_Game.PeriodicalGameTask: Periodical Game Task has started", LogLevel.NORMAL);
	    if (m_areas.IsEmpty())
	    {
	        // Log that no areas exist yet so you can see that the task is running.
	        //Print("IA_Game.PeriodicalGameTask: m_areas is empty. Waiting for initialization.", LogLevel.NORMAL);
	        return;
	    }
	    foreach (IA_AreaInstance areaInst : m_areas)
	    {
				if(!areaInst.m_area){
					//Print("areaInst.m_area is Null!!!",LogLevel.WARNING);
					return;
				}else{
					//Print("areaInst.m_area "+ areaInst.m_area.GetName() +"is NOT Null",LogLevel.NORMAL);
				areaInst.RunNextTask();
				}
	        
	    }
	}
	

    IA_AreaInstance AddArea(IA_Area area, IA_Faction fac, int strength = 0)
    {
		IA_AreaInstance inst = IA_AreaInstance.Create(area, fac, strength);
		if(!inst.m_area){
			//Print("IA_AreaInstance inst.m_area is NULL!", LogLevel.ERROR);
		}
		//Print("Adding Area " + area.GetName(), LogLevel.WARNING);
        m_areas.Insert(inst);
		//Print("m_areas.Count() = " + m_areas.Count(), LogLevel.WARNING);
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep){
            //Print("Adding Area in Replication " + area.GetName(), LogLevel.WARNING);
			rep.AddArea(inst);
		}
        return inst;
    }

    IA_AreaInstance FindAreaInstance(string name)
    {
        foreach (IA_AreaInstance a : m_areas)
        {
            if (a.m_area.GetName() == name)
                return a;
        }
        return null;
    }
};
