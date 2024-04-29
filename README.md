=====================
Simple UDP Server
=====================

technical details:

Distribution of responsibilities for threads:
The server utilizes all existing threads in the system, allocating one thread 
specifically to process network input. The remaining available threads are employed by 
the generator of doubles.

Generator of double notes:
The generator is instantiated by an integer representing the number of threads 
dedicated to it. Each thread operates independently, processing its own sequence of 
requests. The main responsibility of the generator is to ensure a balanced workload 
across all threads. Upon startup, a single thread (further referred to as "Job") 
detaches and continues to operate as long as the Generator object exists; all jobs 
terminate when the generator destructor is called.

As the instance of the generator ages, the Job generates more double values. Once the 
required number of values is generated, the Job forwards the data to a submit callback 
and removes the corresponding instance.

Generator underlying data structure:
To mitigate extensive copying operations and minimize overhead associated with 
validating uniqueness, I have opted for an append-only hash table (albeit considerably 
simplified). In this structure, all pertinent data is stored in a dedicated array, and 
for whole structure total memory overhead of 1.5MB per 1MB of useful data. 
Additionally, a small dynamic part is allocated for resolving collisions. However, 
given the uniform distribution of random numbers, the impact of collisions is minimal. 
Any excess memory is promptly freed once the main data array is filled.

Data transmission and verification:
The data is sent to the client in paginated form, where the entire useful data is 
divided into fixed-size pages. These pages are sent to the client separately to 
streamline the process and avoid inefficient confirmation of receiving every 
individual page. Instead of this, the initial packet has meta-information. Clients do 
not rely  on the meta-information will be first; client will correctly resolve cases 
if the meta-information is not the first or if it is missed entirely.


The meta-information follows a specific scheme. The first two bytes represent the size 
of a single page, followed by a sequence of 8-byte unique page identifiers. In this 
case, the first value in the page serves as the unique identifier because all values 
are unique, making any false positives impossible.
After the completion of data generation, the Job will invoke the submit callback 
responsible for transmitting the data to the client. Additionally, the submit callback 
adds the data to a submit queue. This queue remains active until the client confirms 
the receipt of the data, at which point the data is removed permanently. Essentially, 
this process serves as closing the connection with the client.

On the client side, three possible cases may occur:
    - All data is received properly.
    - Some pages are missing.
    - Checksums are missing.

For each case, the client behaves as follows:
    - If pages are missing, the client requests the missed pages from the server by
    sending a sequence with indexes of the lost pages.

    - If, due to missing checksums, the client cannot determine which pages are lost,
    it sends a sequence of checksums that it has received.

    - If all data is properly received, the client sends a confirmation to the server. 
    This confirmation is sent regardless of whether the data was received in the first 
    iteration or if the client requested resubmission of some pages.


In general, submitting operations do not block each other; they can be concurrent and mixed with resubmitting. However, two resubmitting operations always occur sequentially.

Client-side processing:
The client employs two threads, but it's worth noting that std::sort may utilize 
additional threads for its own operations. The general design of the client is 
structured as follows: the "working" thread waits until the data ready flag is set to 
true. Once set, it sorts the data and writes it to disk. Meanwhile, the 
"communication" thread handles communication with the server, validates the received 
data, and then sets the data ready flag to true when appropriate. 
