// IA_GetInUserAction_Modded.c
// Modded version of SCR_GetInUserAction to enforce role requirements for pilot seats in helicopters.

modded class SCR_GetInUserAction 
{
   
    //------------------------------------------------------------------------------------------------
    override bool CanBePerformedScript(IEntity user)
    {
        BaseCompartmentSlot compartment = GetCompartmentSlot();
        if (!compartment)
            return super.CanBePerformedScript(user); // Should be caught by base class, but good practice

        // Call the new VehicleManager method to check for piloting restrictions
        string reason = "Only Pilots Can Fly!";
        if (!IA_VehicleManager.CanPlayerPilotVehicle(user, compartment, reason))
        {
            SetCannotPerformReason(reason);
            return false;
        }
        
        // Call the original function if our checks pass or don't apply
        return super.CanBePerformedScript(user);
    }
}; 

modded class SCR_SwitchSeatAction
{

    override bool CanBePerformedScript(IEntity user)
    {
		Print("Replication Status = " + Replication.IsServer(),LogLevel.NORMAL);
        BaseCompartmentSlot compartment = GetCompartmentSlot();
        if (!compartment)
            return super.CanBePerformedScript(user); // Should be caught by base class, but good practice

        // Call the new VehicleManager method to check for piloting restrictions
        string reason = "Only Pilots Can Fly!";
        if (!IA_VehicleManager.CanPlayerPilotVehicle(user, compartment, reason))
        {
            SetCannotPerformReason(reason);
            return false;
        }
        
        // Call the original function if our checks pass or don't apply
        return super.CanBePerformedScript(user);
    }

}