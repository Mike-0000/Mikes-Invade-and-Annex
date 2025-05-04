class IA_VectorUtils 
{
    // Sorts an array of vectors in place based on their x-component using Bubble Sort
    static void SortVectorsByX(inout array<vector> positions)
    {
        int n = positions.Count();
        if (n <= 1)
            return; // Already sorted or empty

        bool swapped;
        vector temp;
        for (int i = 0; i < n - 1; i++)
        {
            swapped = false;
            for (int j = 0; j < n - i - 1; j++)
            {
                if (positions[j][0] > positions[j + 1][0])
                {
                    // Swap elements
                    temp = positions[j];
                    positions[j] = positions[j + 1];
                    positions[j + 1] = temp;
                    swapped = true;
                }
            }

            // If no two elements were swapped by inner loop, then break
            if (!swapped)
                break;
        }
    }
} 