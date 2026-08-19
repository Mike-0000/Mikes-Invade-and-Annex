// Combat and group move-planning issue GetOut at high priority. Block the
// group dismount *message* for STATIC_ARTILLERY so idle gunners are not
// ordered off the tube. Do not swallow SCR_AIGetOutVehicle itself: the
// vanilla artillery BT must hop out after aiming to load a shell.

modded class SCR_AIMessageHandling
{
	static override void SendDismountMessage(notnull AIAgent agent, notnull IEntity vehicleEntity, int soldierId, SCR_AIActivityBase relatedActivity,
									notnull AICommunicationComponent myComms, string sendFrom = string.Empty)
	{
		IEntity usageOwner;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(vehicleEntity, usageOwner);
		if (usage && usage.GetVehicleType() == EAIVehicleType.STATIC_ARTILLERY)
			return;

		float dismountDelay = 0.9 * soldierId;

		ref SCR_AIBoardingParameters bParams = new SCR_AIBoardingParameters();
		ref SCR_AIMessage_GetOut msg = SCR_AIMessage_GetOut.Create(vehicleEntity, bParams, relatedActivity, delay_s: dismountDelay);
		msg.SetReceiver(agent);
		myComms.RequestBroadcast(msg, agent);
	}
};
