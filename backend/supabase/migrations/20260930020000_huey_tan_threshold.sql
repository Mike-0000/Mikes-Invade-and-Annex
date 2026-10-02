-- The tan Huey is meant to take dozens of hours of flying. At 2500 a pilot with
-- a full cabin (12 passengers x 30 points) reached it in seven trips. 50000 is
-- 139 full-cabin trips at the best possible rate.
--
-- The seed in 20260930000000_transport_rating.sql now carries the same value;
-- this corrects databases that were seeded with 2500.

UPDATE public.transport_skin_thresholds
   SET required_rating = 50000,
       updated_at      = now()
 WHERE skin_key = 'huey_tan';
